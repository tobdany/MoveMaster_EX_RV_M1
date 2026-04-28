#include <Arduino.h>
#include <ESP32Encoder.h>
//-------------------------------
// --- HANDLER TAREAS ---
//-------------------------------
TaskHandle_t MuestreoHandler;
TaskHandle_t ReadSerialHandler; 
TaskHandle_t WriteSerialHandler;
TaskHandle_t ReadAuxBoardHandler;

//-------------------------------
//--- ESTRUCTURAS ---
//-------------------------------
// Mantiene la lógica completa para el control interno
enum MaqEstado_t {
    MQ_IDLE=1,
    MQ_HOMING=2,
    MQ_MOVIENDO=3,
    MQ_META=4
};

// Reporta a Labview el estado simplificado (Máximo 8 estados)
enum EstadoMotor {
    ST_IDLE = 0,           // 000: Quieto / Esperando
    ST_MOVIENDO = 1,       // 001: Ejecutando trayectoria o comando manual
    ST_HOMING = 2,         // 010: En proceso de búsqueda de cero
    ST_ERROR_TRABADO = 3   // 011: Meta no alcanzada / Obstrucción
};

enum Comportamiento_t {
    MODE_IDLE=0,
    MODE_FAST_MOV=1,
    MODE_MOVE_DEG=2,
    MODE_FOLLOW_ROUTINE=3,
    MODE_HOMING=4
};

enum Homing_t{
  MAQ_HOMING_F1=1,
  MAQ_HOMING_F2=2,
  MAQ_HOMING_F3=3
};

struct Motor_t {
    int pinA;       // Encoder Fase A
    int pinB;       // Encoder Fase B
    int pwmL;       // Salida PWM Izquierda (o Reversa)
    int pwmR;       // Salida PWM Derecha (o Adelante)
    
    // Control de Movimiento
    // Usamos int32_t para que coincida con el getCount() del ESP32Encoder
    int32_t pasosActuales;  
    int32_t metaPasos;
    int32_t errorPasos;
    int32_t ultimoError;
    int32_t pasosHomingAux;
    
    // Estado del Sistema
    int pwmActual;          // El valor final (0-255) que se está mandando

    EstadoMotor estadoReporte;     // Lo que va a LabVIEW
    Comportamiento_t modoActual;   // Cada motor sabe qué está haciendo
    Homing_t faseHoming;           // Fase individual de homing
};

//Estructura para mandar datos de la tarea de muestreo a la tarea de WriteSerial
struct DatosCrudos_t {
    int32_t pasos[6];
    uint8_t finalesCarrera;
    EstadoMotor estado[6];
};

//Estructura para mandar datos a Labview
struct __attribute__((packed)) MsgDatos_t {
     uint8_t header=0x3A;
     uint8_t funcion;
     uint8_t edoMotores[3];
     uint8_t finalesCarrera;
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

//Estrucutra para recibir datos de la tarjeta auxiliar
struct __attribute__((packed)) MsgStatus_Auxiliar_t {
    uint8_t header = 0x3A;
    uint8_t funcion = 0x07; 
    uint8_t payloadSize = 5;
    uint8_t estadoGripper;  
    uint8_t finalesCarrera; 
    uint8_t reserva1 = 0;
    uint8_t reserva2 = 0;
    uint8_t crc;
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
volatile MsgStatus_Auxiliar_t dataAuxiliar;

//-------------------------------
// --- ENCODERS --- 
//-------------------------------

ESP32Encoder encoders[NUM_MOTORES];

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
//Frena el motor
void frenarMotor(Motor_t &objMotor){
  ledcWrite(objMotor.pwmL, 0); 
  ledcWrite(objMotor.pwmR, 0);
}

// Lee los finales de carrera que vienen de la tarjeta auxiliar
bool leerBit(uint8_t byte, int posicion) {
    // Extraemos el bit original (0 si se abrió el circuito/activó, 1 si está cerrado/reposo)
    bool estaPresionado = (byte >> posicion) & 0x01;
    return estaPresionado; // Retornas TRUE si el switch está presionado
}

//Entrega el pwm según los pasos que le faltan
int calcularControl(Motor_t &objMotor) {
    // --- CONSTANTES DE CONTROL ---
    // Como no tienes LabVIEW conectado aún, ajustamos estos valores aquí.
    float Kp = 1.2;  // Ganancia Proporcional (fuerza para llegar)
    float Kd = 0.8;  // Ganancia Derivativa (amortiguación para no pasarse)
    
    // 1. Calcular el error actual
    // (Ya lo calculas en la tarea de muestreo, pero lo aseguramos aquí)
    int32_t error = objMotor.metaPasos - objMotor.pasosActuales;
    
    // 2. Calcular la Derivada (Cambio del error)
    // El periodo es constante (4ms), así que no es estrictamente necesario dividir por dt
    int32_t derivada = error - objMotor.ultimoError;
    
    // 3. Cálculo del PID (solo PD en este caso)
    float salida = (error * Kp) + (derivada * Kd);
    
    // 4. Guardar el error para el próximo ciclo
    objMotor.ultimoError = error;
    
    // 5. Gestión de "Zona Muerta" (Deadzone)
    // Evita que el motor zumbe o intente moverse por errores de 1 o 2 pasos
    if (abs(error) < 3) {
        return 0;
    }

    // 6. PWM Mínimo (Opcional)
    // Los motores del Movemaster necesitan un mínimo de voltaje para vencer la fricción
    int pwmMinimo = 45; 
    if (salida > 0 && salida < pwmMinimo) salida = pwmMinimo;
    if (salida < 0 && salida > -pwmMinimo) salida = -pwmMinimo;

    // 7. Limitar y retornar el resultado
    return (int)constrain(salida, -255, 255);
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

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, xPeriodo);

    DatosCrudos_t datosMuestreo;
    portENTER_CRITICAL(&mux);
    datosMuestreo.finalesCarrera = dataAuxiliar.finalesCarrera;
    portEXIT_CRITICAL(&mux);

    for(int i = 0; i < NUM_MOTORES; i++){
      ArrayMotores[i].pasosActuales = encoders[i].getCount();

      //***************************************************************************************************SIMULACION
      //ArrayMotores[i].pasosActuales  = (i * 1000) + (esp_random() % 1000);

      //***************************************************************************************************SIMULACION2
      //datosMuestreo.pasos[i] = ArrayMotores[i].pasosActuales;

      switch (ArrayMotores[i].modoActual) {
        case MODE_IDLE:
          frenarMotor(ArrayMotores[i]);
          ArrayMotores[i].estadoReporte = ST_IDLE;
          break;

        case MODE_FAST_MOV:{
          // Calculamos el error actual
         int32_t errorActual = ArrayMotores[i].metaPasos - ArrayMotores[i].pasosActuales;
          // Se detiene si está cerca (tolerancia) 
          // O si el signo del error cambió (indicando que ya se pasó de la meta)
          bool yaSePaso = false;
          if (ArrayMotores[i].pwmActual > 0 && errorActual <= 0) yaSePaso = true; // Iba hacia adelante y se pasó
          if (ArrayMotores[i].pwmActual < 0 && errorActual >= 0) yaSePaso = true; // Iba hacia atrás y se pasó

          if (abs(errorActual) < 8 || yaSePaso || leerBit(datosMuestreo.finalesCarrera, i)) { 
              frenarMotor(ArrayMotores[i]);
              ArrayMotores[i].modoActual = MODE_IDLE;
          } else {
              ArrayMotores[i].estadoReporte = ST_MOVIENDO;
              moverMotor(ArrayMotores[i], ArrayMotores[i].pwmActual);
          }
          break;
        }

        case MODE_MOVE_DEG:
         break;

        case MODE_FOLLOW_ROUTINE:
         break;

        case MODE_HOMING:
          ArrayMotores[i].estadoReporte = ST_HOMING;
          switch(ArrayMotores[i].faseHoming){
            case MAQ_HOMING_F1:
              moverMotor(ArrayMotores[i], -50); // Velocidad moderada
              if (leerBit(datosMuestreo.finalesCarrera, i)) {
                frenarMotor(ArrayMotores[i]);
                // Guardamos dónde estamos para saber cuánto alejarnos
                ArrayMotores[i].pasosHomingAux = ArrayMotores[i].pasosActuales;
                ArrayMotores[i].faseHoming = MAQ_HOMING_F2;
              }
              break;

            case MAQ_HOMING_F2:
              moverMotor(ArrayMotores[i], 50); // Mover en sentido contrario
              // Nos alejamos, por ejemplo, 100 pasos
              if (abs(encoders[i].getCount() - ArrayMotores[i].pasosHomingAux) > 100) {
                ArrayMotores[i].faseHoming = MAQ_HOMING_F3;
              }
              break;

            case MAQ_HOMING_F3:
              moverMotor(ArrayMotores[i], -50); // Velocidad muy baja (precisión)
              if (leerBit(datosMuestreo.finalesCarrera, i)) {
                frenarMotor(ArrayMotores[i]);
                encoders[i].clearCount(); // ¡ESTE ES EL CERO REAL!
                ArrayMotores[i].pasosActuales = 0;
                ArrayMotores[i].metaPasos = 0;
                ArrayMotores[i].modoActual = MODE_IDLE;
              }
              break;
           
            break;
          }

      }

      datosMuestreo.pasos[i] =  ArrayMotores[i].pasosActuales;
      datosMuestreo.estado[i] =  ArrayMotores[i].estadoReporte;
      //**********************************************************************************************SIMULACION
      //datosMuestreo.estado[i] =  MOVIENDO;
    }


    //**********************************************************************************************SIMULACION
    datosMuestreo.estado[5] = (EstadoMotor)dataAuxiliar.estadoGripper;
    datosMuestreo.pasos[5] = 0;

    // MANDAR INDIVIDUALMENTE A LA COLA
    xQueueSend(colaTelemetria, &datosMuestreo, 0);
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

          if ((uint8_t)(sumaCrc & 0xFF) == comando.crcFinal) {
            // --- DATOS VÁLIDOS: ACTUALIZAR MOTORES ---
            portENTER_CRITICAL(&mux);
            
            for(int i = 0; i < NUM_MOTORES; i++) {
              Comportamiento_t ordenNueva = (Comportamiento_t)comando.funcion;
              if(ordenNueva == MODE_FAST_MOV && comando.metas[i] != 0) {
                  ArrayMotores[i].modoActual = MODE_FAST_MOV;
                  ArrayMotores[i].metaPasos = ArrayMotores[i].pasosActuales + comando.metas[i];
                  ArrayMotores[i].pwmActual = (comando.metas[i] < 0) ? -(int)comando.payloadSize : (int)comando.payloadSize;
              } 
              else if (ordenNueva == MODE_HOMING) {
                  ArrayMotores[i].modoActual = MODE_HOMING;
                  ArrayMotores[i].faseHoming = MAQ_HOMING_F1;
              }
              else if (ordenNueva == MODE_IDLE) {
                  ArrayMotores[i].modoActual = MODE_IDLE;
              }
                //**********************************************************************************************************SIMULACION
                  //ArrayMotores[i].pasosActuales = comando.metas[i];
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
    DatosCrudos_t datosRecibidos;
    MsgDatos_t msgParaEnviar; 

    for(;;){
        // Recibimos de uno en uno con espera infinita
        if (xQueueReceive(colaTelemetria, &datosRecibidos, portMAX_DELAY) == pdPASS) {
          
          msgParaEnviar.funcion = 0x01; 
          msgParaEnviar.finalesCarrera=datosRecibidos.finalesCarrera;
          memcpy(msgParaEnviar.globalBuffer, datosRecibidos.pasos, 24);

          msgParaEnviar.edoMotores[0] = 0;
          msgParaEnviar.edoMotores[1] = 0;
          msgParaEnviar.edoMotores[2] = 0;

          // Byte 0: [M2_bit0 | M1_bit2-0 | M0_bit2-0] (Aproximadamente)
          msgParaEnviar.edoMotores[0] = (datosRecibidos.estado[0] & 0x07) | 
                                          ((datosRecibidos.estado[1] & 0x07) << 3) |
                                          ((datosRecibidos.estado[2] & 0x03) << 6);

          // Byte 1: [M4_bit1-0 | M3_bit2-0 | M2_bit2-1]
          msgParaEnviar.edoMotores[1] = ((datosRecibidos.estado[2] >> 2) & 0x01) |
                                          ((datosRecibidos.estado[3] & 0x07) << 1) |
                                          ((datosRecibidos.estado[4] & 0x07) << 4) |
                                          ((datosRecibidos.estado[5] & 0x01) << 7);

          // Byte 2: [Libre | M5_bit2-1]
          msgParaEnviar.edoMotores[2] = (datosRecibidos.estado[5] >> 1) & 0x03;

          uint16_t sumaCrc = 0;
          sumaCrc += msgParaEnviar.header;
          sumaCrc += msgParaEnviar.funcion;
          sumaCrc += msgParaEnviar.finalesCarrera;


          for(int i=0; i<3; i++) sumaCrc += msgParaEnviar.edoMotores[i];
          for(int i = 0; i < 24; i++) sumaCrc += msgParaEnviar.globalBuffer[i];
          msgParaEnviar.crcFinal = (uint8_t)(sumaCrc & 0xFF);

          // MANDAR BINARIO DIRECTO
          // LabVIEW recibirá esto conforme llegue y lo guardará en su buffer de entrada
          Serial.write((uint8_t*)&msgParaEnviar, sizeof(MsgDatos_t));
        }
    }
}

//-------------------------------
// --- TAREA : Leer TARJETA AUXILIAR ---
//-------------------------------
void ReadAuxBoard(void * parameter){
    const size_t tamanoPaquete = sizeof(MsgStatus_Auxiliar_t);
    uint8_t buffer[tamanoPaquete];

    for (;;) {
        // ¿Hay suficientes bytes para un paquete completo?
        if (Serial2.available() >= tamanoPaquete) {
            
            // Sincronización: Buscamos el header 0x3A
            if (Serial2.peek() == 0x3A) {
                Serial2.readBytes(buffer, tamanoPaquete);

                // Mapeamos el buffer a nuestra estructura
                MsgStatus_Auxiliar_t temp;
                memcpy(&temp, buffer, tamanoPaquete);

                // --- VALIDACIÓN DE CRC ---
                uint16_t sumaCrc = 0;
                sumaCrc += temp.header;
                sumaCrc += temp.funcion;
                sumaCrc += temp.payloadSize;
                sumaCrc += temp.estadoGripper;
                sumaCrc += temp.finalesCarrera;
                sumaCrc += temp.reserva1;
                sumaCrc += temp.reserva2;

                if ((uint8_t)(sumaCrc & 0xFF) == temp.crc) {
                    uint8_t finalesProcesados = (~temp.finalesCarrera) & 0x1F;

                    // --- SECCIÓN CRÍTICA ---
                    portENTER_CRITICAL(&mux);
                    dataAuxiliar.estadoGripper = temp.estadoGripper;
                    dataAuxiliar.finalesCarrera = finalesProcesados;
                    portEXIT_CRITICAL(&mux);
                }
            } else {
                // Si el primer byte no es el header, lo descartamos y seguimos buscando
                Serial2.read();
            }
        }
        // Pequeño delay para no estresar el núcleo
        vTaskDelay(pdMS_TO_TICKS(10));
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
  const int pinesPWM_L[] = {25, 27, 14, 16, 21};  // 16 es rx2, 17 es tx2
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
      //encoders[i].setFilter(10);
      encoders[i].clearCount();
    } else {
      // El motor 6 no tiene pines de encoder asignados
      ArrayMotores[i].pinA = -1;
      ArrayMotores[i].pinB = -1;
    }

    //Inicializar estrcuctura del motor
    ArrayMotores[i].pasosActuales=0;
    ArrayMotores[i].metaPasos = 0;
    ArrayMotores[i].errorPasos=0;
    ArrayMotores[i].ultimoError = 0;
    ArrayMotores[i].pasosHomingAux=0;
    ArrayMotores[i].estadoReporte = ST_IDLE;
    ArrayMotores[i].modoActual = MODE_IDLE;

    // Configurar PWM (LEDC)
    ledcAttach(ArrayMotores[i].pwmL, frecuencia, resolucion);
    ledcAttach(ArrayMotores[i].pwmR, frecuencia, resolucion);
   
  }

  Serial.println("[SETUP] Pines configurados.");
// --- Asignación de memoria a la colaTelemetría ---
  colaTelemetria = xQueueCreate(100, sizeof(DatosCrudos_t));
  if (colaTelemetria == NULL) {
        Serial.println("Error al crear la cola");
  }

  //Serial.println("[SETUP] Creando tareas...");

// --- CREAMOS LAS TAREAS EN NÚCLEOS DIFERENTES
  xTaskCreatePinnedToCore(tareaMuestreo, "TaskMuestreo", 4096, NULL, 3, &MuestreoHandler, 1);
  xTaskCreatePinnedToCore(ReadSerial, "TaskReadSerial", 4096, NULL, 1, &ReadSerialHandler, 0);
  xTaskCreatePinnedToCore(WriteSerial, "TaskWriteSerial", 4096, NULL, 1, &WriteSerialHandler, 0);
  xTaskCreatePinnedToCore(ReadAuxBoard,"ReadAuxiliarBoard",4096,NULL,2,&ReadAuxBoardHandler,0);
  //Serial.println("[SETUP] Todo listo. Entrando a loop.");
}

void loop() {
  // El loop principal se queda vacío porque estamos usando Tasks
  delay(1000);
}