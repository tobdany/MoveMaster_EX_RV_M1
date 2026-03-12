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

struct __attribute__((packed)) MsgDatos_t {
     uint8_t header=0x3A;
     uint8_t funcion;
     uint8_t payloadSize=25;
     uint8_t globalBuffer[24];
     uint8_t crcFinal;
};

//-------------------------------
// -- MOTORES ---
//-------------------------------
const int NUM_MOTORES = 6;
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
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriodo = pdMS_TO_TICKS(2); 
  
  uint8_t localBuffer[24]; 
  MsgDatos_t arrayMsg[25];
  int iteradorMsg = 0;

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, xPeriodo);
    
    uint16_t sumaCrc = 0;
    memset(localBuffer, 0, 24); // Limpiamos buffer
    
    // 1. Recolectar datos de los 6 motores (o los que tengas)
    for(int i = 0; i < NUM_ENCODERS; i++){
      int32_t pasos = encoders[i].getCount();
      memcpy(&localBuffer[i * 4], &pasos, 4);
    }

    // 2. Preparar el mensaje individual
    MsgDatos_t clusterLocalMsg;
    clusterLocalMsg.funcion = 0x01;
    memcpy(clusterLocalMsg.globalBuffer, localBuffer, 24);
    
    // Calcular CRC simple
    for(int i = 0; i < 24; i++) sumaCrc += localBuffer[i];
    clusterLocalMsg.crcFinal = (uint8_t)(sumaCrc & 0xFF);

    // 3. Llenar la ráfaga de 25
    arrayMsg[iteradorMsg] = clusterLocalMsg;
    iteradorMsg++;

    if(iteradorMsg >= 25){
      if (xQueueSend(colaTelemetria, &arrayMsg, 0) != pdPASS) {
          // Cola llena: el consumidor (Serial) es más lento que el productor
      }
      iteradorMsg = 0;
    }
  }
}
//-------------------------------
// --- TAREA : Leer Consola ---
//-------------------------------
void ReadSerial(void * parameter) {
  int pwm=0;
  int vueltas=0;

  for (;;) {
    // 1. LEER DE LA CONSOLA (Comandos desde LabVIEW)
    if (Serial.available() > 0) {
      String cmd = Serial.readStringUntil('\n');
      if (cmd.startsWith("HOME:INIT:")) {
        pwm = cmd.substring(10).toInt();
        vueltas=-1;
        //lógica para mandar el comando
      }

      else if(cmd.startsWith("MOV:1V:")){
        pwm=cmd.substring(7).toInt();
        vueltas=1;
      }

      else if(cmd.startsWith("MOV:10V:")){
        pwm=cmd.substring(8).toInt(); 
        vueltas=10;
      }

      Serial.print("pwm: ");
      Serial.print(pwm);
      Serial.print(" , vueltas:");
      Serial.println(vueltas);

    // 2. ENVIAR DATOS (Tus paquetes de 500 bytes o lo que decidas)
    portENTER_CRITICAL(&mux);
    for(int i=0; i<NUM_MOTORES; i++){
      ArrayMotores[i].pwmActual = pwm;
    }
    portEXIT_CRITICAL(&mux);

    // 3. EJECUTAR MOVIMIENTO (Fuera de la sección crítica)
    for(int i=0; i<NUM_MOTORES; i++){
      moverMotor(ArrayMotores[i], ArrayMotores[i].pwmActual);
    }

    }
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
  
}

//-------------------------------
// --- TAREA:LEER AL SERIAL
////-------------------------------

void WriteSerial(void * parameter){
  MsgDatos_t arrayLocalRecibido[25];
  
  for(;;){
    if (xQueueReceive(colaTelemetria, &arrayLocalRecibido, portMAX_DELAY) == pdPASS) {
      //poner la lógica de labview
      
      int32_t* pasos = (int32_t*)arrayLocalRecibido[0].globalBuffer;
      Serial.printf("P_M1:%d, P_M2:%d, P_M3:%d, P_M4:%d, P_M5:%d, P_M6:N/A\n", 
                    pasos[0], pasos[1], pasos[2], pasos[3], pasos[4]);
    }
  }
}

//-------------------------------
// TAREA: SETUP
//-------------------------------

void setup() {

// --- Comunicación serial con Labview
  Serial.begin(921600);

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
  const int pinesEncoder[5][2] = {
    {34, 35}, // Motor 1 (Recordatorio: pon las R de 10k externas)
    {36, 39}, // Motor 2
    {32, 33}, // Motor 3
    {18, 19}, // Motor 4
    {23, 05}  // Motor 5
  };

  //const int arrayPasosPorVuelta[]={400,400,400,96,96,0};
  const int arrayPasosPorVuelta[]={400,400,400,400,400,400};

// --- ASIGNACIÓN DE PINES ---
  for(int i = 0; i < NUM_MOTORES; i++) {
    //Configurar hardware en la estructura
    ArrayMotores[i].pwmL = pinesPWM_L[i];
    ArrayMotores[i].pwmR = pinesPWM_R[i];

    //Encoders
    if(i < NUM_ENCODERS) {
      ArrayMotores[i].pinA = pinesEncoder[i][0];
      ArrayMotores[i].pinB = pinesEncoder[i][1];
      
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

// --- Asignación de memoria a la colaTelemetría ---
  colaTelemetria = xQueueCreate(5, sizeof(MsgDatos_t) * 25);
  if (colaTelemetria == NULL) {
        Serial.println("Error al crear la cola");
  }



// --- CREAMOS LAS TAREAS EN NÚCLEOS DIFERENTES
  xTaskCreatePinnedToCore(tareaMuestreo, "TaskMuestreo", 4096, NULL, 3, &MuestreoHandler, 1);
  xTaskCreatePinnedToCore(ReadSerial, "TaskReadSerial", 4096, NULL, 1, &ReadSerialHandler, 0);
  xTaskCreatePinnedToCore(WriteSerial, "TaskWriteSerial", 4096, NULL, 1, &WriteSerialHandler, 0);
}

void loop() {
  // El loop principal se queda vacío porque estamos usando Tasks
  delay(1000);
}