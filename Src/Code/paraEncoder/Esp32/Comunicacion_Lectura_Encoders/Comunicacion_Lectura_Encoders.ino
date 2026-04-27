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
enum EstadoMotor {
    IDLE,        // Quieto, esperando orden
    MOVIENDO,    // En trayectoria
    META_ALCANZADA, // Se trabó o algo pasó
    HOMING_FASE1, // Búsqueda rápida
    HOMING_FASE2, // Alejarse
    HOMING_FASE3,  // Búsqueda lenta
    COMANDO_MANUAL
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
    bool dirActual;         // true = Adelante, false = Atrás
    EstadoMotor estado;     // IDLE, MOVIENDO, etc.
};

//Estructura para mandar datos de la tarea de muestreo a la tarea de WriteSerial
struct DatosCrudos_t {
    int32_t pasos[6];
    uint8_t finalesCarrera;
    EstadoMotor estado[6];;
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
//Frena el motor
void frenarMotor(Motor_t &objMotor){
  ledcWrite(objMotor.pwmL, 0); 
  ledcWrite(objMotor.pwmR, 0);
}

// Lee los finales de carrera que vienen de la tarjeta auxiliar
bool leerBit(uint8_t byte, int posicion) {
    // Extraemos el bit original (0 si se abrió el circuito/activó, 1 si está cerrado/reposo)
    bool estaPresionado = (dataAuxiliar.finalesCarrera >> posicion) & 0x01;
   
    /*if(estaPresionado) { 
        if(posicion == 0) frenarMotor(ArrayMotores[0]); // Cadera
        if(posicion == 1 || posicion == 2) frenarMotor(ArrayMotores[3]); // Codo
        if(posicion == 3 || posicion == 4) frenarMotor(ArrayMotores[4]); // Antebrazo
    }*/

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
    datosMuestreo.finalesCarrera = dataAuxiliar.finalesCarrera;

    for(int i = 0; i < NUM_MOTORES; i++){
      ArrayMotores[i].pasosActuales = encoders[i].getCount();

      //***************************************************************************************************SIMULACION
      //ArrayMotores[i].pasosActuales  = (i * 1000) + (esp_random() % 1000);

      //***************************************************************************************************SIMULACION2
      //datosMuestreo.pasos[i] = ArrayMotores[i].pasosActuales;

      switch (ArrayMotores[i].estado) {
            
        case IDLE:
          frenarMotor(ArrayMotores[i]);
          // Se queda aquí hasta que ReadSerial cambie el estado a MOVIENDO o HOMING
          break;

        case MOVIENDO:
          if(leerBit(dataAuxiliar.finalesCarrera, i)){
              ArrayMotores[i].estado = IDLE;
              frenarMotor(ArrayMotores[i]);
              break;
          }
          // Calcular error
          ArrayMotores[i].errorPasos = ArrayMotores[i].metaPasos - ArrayMotores[i].pasosActuales;

          // Si el error es pequeño, llegamos a la meta
          if (abs(ArrayMotores[i].errorPasos) < 5) { // Tolerancia de 5 pasos
              ArrayMotores[i].estado = META_ALCANZADA;
          } else {
              // Aquí llamarías a tu función de control (ej. un PD simple)
              int pwmCalculado = calcularControl(ArrayMotores[i]); 
              moverMotor(ArrayMotores[i], pwmCalculado);
          }
          break;

        case HOMING_FASE1: // BÚSQUEDA RÁPIDA
          moverMotor(ArrayMotores[i], -100); // Velocidad moderada
          if (leerBit(dataAuxiliar.finalesCarrera, i)) {
              frenarMotor(ArrayMotores[i]);
              // Guardamos dónde estamos para saber cuánto alejarnos
              ArrayMotores[i].pasosHomingAux = encoders[i].getCount(); 
              ArrayMotores[i].estado = HOMING_FASE2; 
          }
            
          break;

        case HOMING_FASE2: // ALEJARSE (BACK-OFF)
          moverMotor(ArrayMotores[i], 80); // Mover en sentido contrario
          // Nos alejamos, por ejemplo, 100 pasos
          if (abs(encoders[i].getCount() - ArrayMotores[i].pasosHomingAux) > 100) {
              frenarMotor(ArrayMotores[i]);
              ArrayMotores[i].estado = HOMING_FASE3;
          }
          break;

        case HOMING_FASE3: // TOQUE LENTO FINAL
          moverMotor(ArrayMotores[i], -50); // Velocidad muy baja (precisión)
          if (leerBit(dataAuxiliar.finalesCarrera, i)) {
              frenarMotor(ArrayMotores[i]);
              encoders[i].clearCount(); // ¡ESTE ES EL CERO REAL!
              ArrayMotores[i].pasosActuales = 0;
              ArrayMotores[i].metaPasos = 0;
              ArrayMotores[i].estado = IDLE; 
          }
          break;

        case COMANDO_MANUAL:
        {
          // Calculamos el error actual
          int32_t errorActual = ArrayMotores[i].metaPasos - ArrayMotores[i].pasosActuales;
          // Se detiene si está cerca (tolerancia) 
          // O si el signo del error cambió (indicando que ya se pasó de la meta)
          bool yaSePaso = false;
          if (ArrayMotores[i].pwmActual > 0 && errorActual <= 0) yaSePaso = true; // Iba hacia adelante y se pasó
          if (ArrayMotores[i].pwmActual < 0 && errorActual >= 0) yaSePaso = true; // Iba hacia atrás y se pasó

          if (abs(errorActual) < 8 || yaSePaso) { 
              frenarMotor(ArrayMotores[i]);
              ArrayMotores[i].estado = IDLE; 
          } else {
              // Seguimos moviendo a la velocidad definida por LabVIEW
              moverMotor(ArrayMotores[i], ArrayMotores[i].pwmActual);
          }
          
          // Seguridad por finales de carrera
          if(leerBit(dataAuxiliar.finalesCarrera, i)){
              frenarMotor(ArrayMotores[i]);
              ArrayMotores[i].estado = IDLE;
          }
          break;
        }

        case META_ALCANZADA: // Se manda a llamar cuando los pasos no han cambiado
          // Estado de seguridad o bloqueo
          frenarMotor(ArrayMotores[i]);
          break;

        }

      datosMuestreo.pasos[i] =  ArrayMotores[i].pasosActuales;
      datosMuestreo.estado[i] =  ArrayMotores[i].estado;
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
              //Función 3:  Se presionaron las flechas del control VI <- ->
              if(comando.funcion==3){
                  if(comando.metas[i] != 0){
                    ArrayMotores[i].estado = COMANDO_MANUAL;
                    ArrayMotores[i].metaPasos = ArrayMotores[i].pasosActuales + comando.metas[i];
        
                    // Asignación de PWM con signo
                    if(comando.metas[i] < 0) {
                      ArrayMotores[i].pwmActual = -((int)comando.payloadSize); 
                    } else {
                      ArrayMotores[i].pwmActual = (int)comando.payloadSize; 
                    }
                  }

                }else{

                  //Función normal
                  ArrayMotores[i].metaPasos = comando.metas[i];
                  ArrayMotores[i].estado = MOVIENDO; // Activa el PID

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
                memcpy((void*)&dataAuxiliar, buffer, tamanoPaquete);

                dataAuxiliar.finalesCarrera = (~dataAuxiliar.finalesCarrera) & 0x1F;

                // --- VALIDACIÓN DE CRC ---
                uint16_t sumaCrc = 0;
                sumaCrc += dataAuxiliar.header;
                sumaCrc += dataAuxiliar.funcion;
                sumaCrc += dataAuxiliar.payloadSize;
                sumaCrc += dataAuxiliar.estadoGripper;
                sumaCrc += dataAuxiliar.finalesCarrera;
                sumaCrc += dataAuxiliar.reserva1;
                sumaCrc += dataAuxiliar.reserva2;

                for(int i = 0; i < 5; i++) {
                  leerBit(dataAuxiliar.finalesCarrera, i);
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
    ArrayMotores[i].estado = IDLE;
    ArrayMotores[i].pwmActual = 0;
    ArrayMotores[i].dirActual = false;

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