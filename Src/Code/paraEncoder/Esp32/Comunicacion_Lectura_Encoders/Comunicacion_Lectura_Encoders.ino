#include <Arduino.h>
#include <ESP32Encoder.h>
//-------------------------------
// --- HANDLER TAREAS ---
//-------------------------------
TaskHandle_t MuestreoHandler;
TaskHandle_t ReadSerialHandler; 
TaskHandle_t WriteSerialHandler;

//-------------------------------
//--- ESTRUCTURAS ---
//-------------------------------
enum EstadoMotor {
    IDLE,        // Quieto, esperando orden
    MOVIENDO,    // En trayectoria
    META_ALCANZADA, // Se trabó o algo pasó
    HOMING       // Buscando el cero
};

struct Motor_t {
    int pinA;       // Encoder Fase A
    int pinB;       // Encoder Fase B
    int pwmL;       // Salida PWM Izquierda (o Reversa)
    int pwmR;       // Salida PWM Derecha (o Adelante)
    
    // Configuración Mecánica
    int pasosPorVuelta; 
    
    // Control de Movimiento
    // Usamos int32_t para que coincida con el getCount() del ESP32Encoder
    int32_t pasosActuales;  
    int32_t metaPasos;
    int32_t errorPasos;     // Útil para tu PID o control de llegada
    
    // Estado del Sistema
    int pwmActual;          // El valor final (0-255) que se está mandando
    bool dirActual;         // true = Adelante, false = Atrás
    EstadoMotor estado;     // IDLE, MOVIENDO, etc.
};

//Estructura para mandar datos a Labview
struct __attribute__((packed)) MsgDatos_t {
     uint8_t header=0x3A;
     uint8_t funcion;
     uint8_t payloadSize=25;
     uint8_t globalBuffer[24];
     uint8_t crcFinal;
};


//Estructura para recibir comandos de Labview
struct __attribute__((packed)) MsgComando_t {
    uint8_t header;      // 0x3A
    uint8_t funcion;     // 0x02
    uint8_t payloadSize; // 24
    int32_t metas[6];    // 24 bytes (metas para los motores)
    uint8_t crcFinal;
};

//-------------------------------
// -- MOTORES ---
//-------------------------------
const int NUM_MOTORES = 5;
const int NUM_ENCODERS = 5; 

//-------------------------------
// -- QUEUE ---
//-------------------------------
QueueHandle_t colaTelemetria;

//-------------------------------
// --- GLOBALES COMPARTIDAS ---
//-------------------------------

volatile long pasosCompartidos = 0;
volatile int pwmObjetivo = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // Para evitar conflictos entre núcleos

Motor_t ArrayMotores[NUM_MOTORES];
MsgDatos_t structMensaje;


//-------------------------------
// --- ENCODERS --- 
//-------------------------------

ESP32Encoder encoders[NUM_MOTORES];
const int PASOS_POR_VUELTA = 400;

//-------------------------------
// --- FUNCIONES --- 
//-------------------------------
void moverMotor(Motor_t &objMotor, int velocidad) {
    // Limitamos la velocidad entre -255 y 255 (Rango de 8 bits)
    int velProtegida = constrain(velocidad, -255, 255);

    if (velProtegida < 0) {
        // Usamos abs() para mandar el valor positivo al PWM
        ledcWrite(objMotor.pwmL, abs(velProtegida)); 
        ledcWrite(objMotor.pwmR, 0);
    } else {
        ledcWrite(objMotor.pwmR, velProtegida); 
        ledcWrite(objMotor.pwmL, 0);
    }
}

void frenarMotor(Motor_t &objMotor){
  ledcWrite(objMotor.pwmL, 0); 
  ledcWrite(objMotor.pwmR, 0);
}



//-------------------------------
// ---TAREA: CONTROL Y ENCODER ---
/*
* Manda datos a la tarea de escribir al serial
*/
//-------------------------------

void tareaMuestreo(void * parameter) {
  //Serial.println("[TASK] Muestreo Iniciada");
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriodo = pdMS_TO_TICKS(4); 
    uint8_t localBuffer[24]; 

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xPeriodo);
        memset(localBuffer, 0, 24); 
        
        for(int i = 0; i < NUM_ENCODERS; i++){
          //***********************************************************************************SIMULACION
            int32_t pasos = ArrayMotores[i].pasosActuales;
           // int32_t pasos = encoders[i].getCount();
            // Usamos datos simulados o reales de los encoders
            //int32_t pasos = (i * 1000) + (esp_random() % 1000);
            memcpy(&localBuffer[i * 4], &pasos, 4);
        }

        MsgDatos_t clusterLocalMsg;
        clusterLocalMsg.funcion = 0x01; 
        memcpy(clusterLocalMsg.globalBuffer, localBuffer, 24);

        uint16_t sumaCrc = 0;
        sumaCrc += clusterLocalMsg.header;
        sumaCrc += clusterLocalMsg.funcion;
        sumaCrc += clusterLocalMsg.payloadSize;
        for(int i = 0; i < 24; i++) sumaCrc += clusterLocalMsg.globalBuffer[i];
        clusterLocalMsg.crcFinal = (uint8_t)(sumaCrc & 0xFF);

        // MANDAR INDIVIDUALMENTE A LA COLA
        // Ya no llenamos un array de 25 aquí
        if (xQueueSend(colaTelemetria, &clusterLocalMsg, 0) != pdPASS) {
            // Si la cola se llena, se descarta el dato para no bloquear el núcleo
        }
    }
}
//-------------------------------
// --- TAREA : Leer Consola ---
//-------------------------------
void ReadSerial(void * parameter) {
    MsgComando_t comando;
    const size_t tamanoPaquete = sizeof(MsgComando_t);
    uint8_t buffer[tamanoPaquete];

    for (;;) {
        // ¿Hay suficientes bytes para un paquete completo?
        if (Serial.available() >= tamanoPaquete) {
            
            // Sincronización: Buscamos el header 0x3A
            if (Serial.peek() == 0x3A) {
                Serial.readBytes(buffer, tamanoPaquete);

                // Mapeamos el buffer a nuestra estructura
                memcpy(&comando, buffer, tamanoPaquete);

                // --- VALIDACIÓN DE CRC ---
                uint16_t sumaCrc = 0;
                sumaCrc += comando.header;
                sumaCrc += comando.funcion;
                sumaCrc += comando.payloadSize;
                for(int i = 0; i < 24; i++) {
                    sumaCrc += ((uint8_t*)comando.metas)[i];
                }

                if(true){
                  
                //if ((uint8_t)(sumaCrc & 0xFF) == comando.crcFinal) {
                    // --- DATOS VÁLIDOS: ACTUALIZAR MOTORES ---

                    portENTER_CRITICAL(&mux);
                    for(int i = 0; i < NUM_MOTORES; i++) {
                        // Actualizamos la meta de pasos para cada motor
                        ArrayMotores[i].metaPasos = comando.metas[i];
                        ArrayMotores[i].estado = MOVIENDO;

                        //**********************************************************************************************************SIMULACION
                        ArrayMotores[i].pasosActuales = comando.metas[i];
                    }
                    portEXIT_CRITICAL(&mux);
                } 
            } else {
                // Si el primer byte no es el header, lo descartamos y seguimos buscando
                Serial.read();
            }
        }
        // Pequeño delay para no estresar el núcleo
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

//-------------------------------
// --- TAREA:ESCRIBIR AL SERIAL
////-------------------------------

void WriteSerial(void * parameter){
  //Serial.println("[TASK] WriteSerial Iniciada");

    MsgDatos_t msgParaEnviar; 
    
    for(;;){
        // Recibimos de uno en uno con espera infinita
        if (xQueueReceive(colaTelemetria, &msgParaEnviar, portMAX_DELAY) == pdPASS) {
            // MANDAR BINARIO DIRECTO
            // LabVIEW recibirá esto conforme llegue y lo guardará en su buffer de entrada
           Serial.write(msgParaEnviar.header); 
           Serial.write(msgParaEnviar.funcion); 
           Serial.write(msgParaEnviar.payloadSize);      
           Serial.write(msgParaEnviar.globalBuffer, 24); 
           Serial.write(msgParaEnviar.crcFinal);
        }
    }
}


//-------------------------------
// TAREA: SETUP
//-------------------------------

void setup() {

// --- Comunicación serial con Labview
  Serial.begin(921600);
  delay(1000); // ESPERA 2 SEGUNDOS CRÍTICOS
 // Serial.println("\n\n*******************************");
 // Serial.println("PRUEBA DE ARRANQUE FORZADA");
 // Serial.println("*******************************");

 // Serial.println("\n--- SISTEMA INICIANDO ---");

// --- Comunicación serial con la otra tarjeta
  const int RXD2=13;
  const int TXD2=12;
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

//--- Pines del PWM
  const int frecuencia = 5000;
  const int resolucion = 8;
  const int pinesPWM_L[] = {25, 27, 14, 16, 21}; 
  const int pinesPWM_R[] = {26, 04, 15, 17, 02}; 



// --- ENCODERS ---
  ESP32Encoder::useInternalWeakPullResistors = puType::none;
  const int pinesEncoder[5][2] = {
    {34, 35}, // Motor 1 
    {36, 39}, // Motor 2  (VP/VN)
    {32, 33}, // Motor 3
    {18, 19}, // Motor 4
    {23, 05}  // Motor 5
  };

  //const int arrayPasosPorVuelta[]={400,400,400,96,96,0};
  const int arrayPasosPorVuelta[]={400,400,400,192,192,0};

// --- ASIGNACIÓN DE PINES ---
  for(int i = 0; i < NUM_MOTORES; i++) {
    //Configurar hardware en la estructura
    ArrayMotores[i].pwmL = pinesPWM_L[i];
    ArrayMotores[i].pwmR = pinesPWM_R[i];
    

    //Encoders
    if(i < NUM_ENCODERS) {
      ArrayMotores[i].pinA = pinesEncoder[i][0];
      ArrayMotores[i].pinB = pinesEncoder[i][1];
      /*pinMode(ArrayMotores[i].pinA, INPUT);
      pinMode(ArrayMotores[i].pinB, INPUT);   */   
      /*Serial.print("num encoder: ");
      Serial.println(i);*/
      
      encoders[i].attachHalfQuad(ArrayMotores[i].pinA, ArrayMotores[i].pinB);
      encoders[i].setFilter(10);
      encoders[i].clearCount();
    } else {
      // El motor 6 no tiene pines de encoder asignados
      ArrayMotores[i].pinA = -1;
      ArrayMotores[i].pinB = -1;
    }

    //Inicializar estados
    ArrayMotores[i].estado = IDLE;
    ArrayMotores[i].metaPasos = 0;
    
    // Configurar PWM (LEDC)
    ledcAttach(ArrayMotores[i].pwmL, frecuencia, resolucion);
    ledcAttach(ArrayMotores[i].pwmR, frecuencia, resolucion);
   
  }

  Serial.println("[SETUP] Pines configurados.");
// --- Asignación de memoria a la colaTelemetría ---
  colaTelemetria = xQueueCreate(100, sizeof(MsgDatos_t));
  if (colaTelemetria == NULL) {
        Serial.println("Error al crear la cola");
  }

  //Serial.println("[SETUP] Creando tareas...");

// --- CREAMOS LAS TAREAS EN NÚCLEOS DIFERENTES
  xTaskCreatePinnedToCore(tareaMuestreo, "TaskMuestreo", 4096, NULL, 3, &MuestreoHandler, 1);
  xTaskCreatePinnedToCore(ReadSerial, "TaskReadSerial", 4096, NULL, 1, &ReadSerialHandler, 0);
  xTaskCreatePinnedToCore(WriteSerial, "TaskWriteSerial", 4096, NULL, 1, &WriteSerialHandler, 0);

  //Serial.println("[SETUP] Todo listo. Entrando a loop.");
}

void loop() {
  // El loop principal se queda vacío porque estamos usando Tasks
  delay(1000);
}