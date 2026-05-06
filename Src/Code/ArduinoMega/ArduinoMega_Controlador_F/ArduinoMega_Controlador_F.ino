#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/atomic.h>


// --- CONFIGURACIÓN DE PINES ---
const uint8_t NUM_MOTORES = 6;
const uint8_t NUM_ENCODERS = 5;
const uint8_t PIN_LPWM[NUM_MOTORES] = { 2, 4, 6, 8, 10, 12 };
const uint8_t PIN_RPWM[NUM_MOTORES] = { 3, 5, 7, 9, 11, 13 };
const uint8_t PIN_ENCODER_A[NUM_ENCODERS] = { 39, 35, 31, 27, 23 };
const uint8_t PIN_ENCODER_B[NUM_ENCODERS] = { 41, 37, 33, 29, 25 };
const uint8_t PIN_FRENO[2] = { 14, 15 };

// --- ENUMS ---
// Mantiene la lógica completa para el control interno
enum MaqEstado_t {
  MQ_IDLE = 1,
  MQ_HOMING = 2,
  MQ_MOVIENDO = 3,
  MQ_META = 4
};

// Reporta a Labview el estado simplificado (Máximo 8 estados)
enum EstadoMotor {
  ST_IDLE = 0,          // 000: Quieto / Esperando
  ST_MOVIENDO = 1,      // 001: Ejecutando trayectoria o comando manual
  ST_HOMING = 2,        // 010: En proceso de búsqueda de cero
  ST_ERROR_TRABADO = 3  // 011: Meta no alcanzada / Obstrucción
};

enum Comportamiento_t {
  MODE_IDLE = 0,
  MODE_FAST_MOV = 1,
  MODE_MOVE_DEG = 2,
  MODE_FOLLOW_ROUTINE = 3,
  MODE_HOMING = 4
};

enum Homing_t {
  MAQ_HOMING_F1 = 1,
  MAQ_HOMING_F2 = 2,
  MAQ_HOMING_F3 = 3
};

// --- ESTRUCTURAS ---
//Estructura para mandar datos a Labview
struct __attribute__((packed)) MsgDatos_t {
  uint8_t header = 0x3A;
  uint8_t funcion = 0x01;
  uint8_t edoMotores[3] = { 0, 0, 0 };
  uint8_t finalesCarrera = 0;
  int32_t pasos[6] = { 0, 0, 0, 0, 0, 0 };
  uint8_t crcFinal;
};

// Manda Datos a Labview
struct __attribute__((packed)) MsgComando_t {
  uint8_t header;
  uint8_t funcion;
  uint8_t payloadSize;
  int32_t metas[6];
  uint8_t crcFinal;
};

//Estructura del motor
struct Motor_t {
  int pinA;  // Encoder Fase A
  int pinB;  // Encoder Fase B
  int pwmL;  // Salida PWM Izquierda (o Reversa)
  int pwmR;  // Salida PWM Derecha (o Adelante)

  // Control de Movimiento
  // Usamos int32_t para que coincida con el getCount() del ESP32Encoder
  int32_t pasosActuales;
  int32_t metaPasos;
  int32_t errorPasos;
  int32_t ultimoError;
  int32_t pasosHomingAux;
  int32_t ticksRestantes;  // Contador para el timer

  // Estado del Sistema
  int pwmActual;  // El valor final (0-255) que se está mandando


  EstadoMotor estadoReporte;    // Lo que va a LabVIEW
  Comportamiento_t modoActual;  // Cada motor sabe qué está haciendo
  Homing_t faseHoming;          // Fase individual de homing
};

struct EncoderHw_t {
  volatile uint8_t *portA, *portB;
  uint8_t maskA, maskB;
};

// --- VARIABLES GLOBALES ---
volatile int32_t encoderCount[NUM_ENCODERS] = { 0, 0, 0, 0, 0 };
volatile uint8_t encoderLastState[NUM_ENCODERS];
EncoderHw_t encoderHw[NUM_ENCODERS];
Motor_t ArrayMotores[NUM_MOTORES];

// Tabla de verdad para resolución X2 (400 pulsos)
// Solo reacciona a cambios en una fase para reducir la resolución a la mitad
const int8_t encoderTable[16] = {
  0, 0, 0, 0,   // Sin cambio
  1, 0, 0, -1,  // Cambios detectados
  -1, 0, 0, 1,  // Cambios detectados
  0, 0, 0, 0    // Sin cambio
};

uint32_t lastControlUs = 0;
const uint32_t CONTROL_PERIOD_US = 4000UL;  // 20ms para esta prueba (más lento es mejor para ver datos)

// --- LÓGICA DE ENCODERS (TIMER 5) ---
ISR(TIMER5_COMPA_vect) {
  for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
    uint8_t a = (*(encoderHw[i].portA) & encoderHw[i].maskA) ? 1 : 0;
    uint8_t b = (*(encoderHw[i].portB) & encoderHw[i].maskB) ? 1 : 0;
    uint8_t estadoActual = (a << 1) | b;

    // Actualizar la variable dentro de la estructura
    ArrayMotores[i].pasosActuales += encoderTable[(encoderLastState[i] << 2) | estadoActual];
    encoderLastState[i] = estadoActual;
  }
}

void configurarTimer5() {
  noInterrupts();
  TCCR5A = 0;
  TCCR5B = (1 << WGM52) | (1 << CS51);  // CTC, prescaler 8
  OCR5A = 199;                          // Cambia a 199 para 10kHz (más aire para el CPU)
  TIMSK5 = (1 << OCIE5A);
  interrupts();
}

// --- CONTROL DE MOTORES ---
void frenarMotor(Motor_t &m) {
  analogWrite(m.pwmL, 0);
  analogWrite(m.pwmR, 0);
}


void moverMotor(Motor_t &m, int16_t v) {
  //digitalWrite(PIN_FRENO[0], LOW);
  //digitalWrite(PIN_FRENO[1], LOW);

  if (v > 0) {
    analogWrite(m.pwmR, v);
    analogWrite(m.pwmL, 0);
  } else if (v < 0) {
    analogWrite(m.pwmR, 0);
    analogWrite(m.pwmL, -v);
  } else {
    frenarMotor(m);
  }
}



//--- LECTURA DE COMANDOS DESDE LABVIEW ---
void procesarLecturaSerial() {
  const size_t tamanoPaquete = sizeof(MsgComando_t);
  static uint8_t buffer[sizeof(MsgComando_t)];

  //if (Serial.available() >= tamanoPaquete) {
  while (Serial.available() > 0) {
    if (Serial.peek() == 0x3A) {
      if (Serial.available() >= tamanoPaquete) {
        Serial.readBytes(buffer, tamanoPaquete);
        MsgComando_t comando;
        memcpy(&comando, buffer, tamanoPaquete);

        uint16_t sumaCrc = 0;
        sumaCrc += comando.header;
        sumaCrc += comando.funcion;
        sumaCrc += comando.payloadSize;
        for (int i = 0; i < 24; i++) {
          sumaCrc += ((uint8_t *)comando.metas)[i];
        }

        if ((uint8_t)(sumaCrc & 0xFF) == comando.crcFinal) {
          ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            for (int i = 0; i < NUM_MOTORES; i++) {
              Comportamiento_t ordenNueva = (Comportamiento_t)comando.funcion;

              if (ordenNueva == MODE_FAST_MOV && comando.metas[i] != 0) {
                ArrayMotores[i].modoActual = MODE_FAST_MOV;
                ArrayMotores[i].metaPasos = ArrayMotores[i].pasosActuales + comando.metas[i];
                ArrayMotores[i].pwmActual = (comando.metas[i] < 0) ? -(int)comando.payloadSize : (int)comando.payloadSize;
              } else if (ordenNueva == MODE_HOMING) {
                ArrayMotores[i].modoActual = MODE_HOMING;
                ArrayMotores[i].faseHoming = MAQ_HOMING_F1;
              } else if (ordenNueva == MODE_IDLE) {
                ArrayMotores[i].modoActual = MODE_IDLE;
              } else if (ordenNueva == MODE_FOLLOW_ROUTINE) {
                int16_t segundos = (int16_t)(comando.metas[i] & 0xFFFF);
                int16_t pwmValue = (int16_t)((comando.metas[i] >> 16) & 0xFFFF);

                if (segundos > 0) {
                  ArrayMotores[i].modoActual = MODE_FOLLOW_ROUTINE;
                  ArrayMotores[i].pwmActual = pwmValue;
                  ArrayMotores[i].ticksRestantes = (int32_t)segundos * 250;
                } else {
                  ArrayMotores[i].modoActual = MODE_IDLE;
                }
              }  // Cierra else if Routine
            }    // Cierra el FOR de motores
          }      // Cierra IF del CRC
        } else {
          break;
        }  //Cierra bloque atómico
      } // Cierra lo del crc
    } else {
      Serial.read();  // Descartar si no es el header
    }
  }  // Cierra IF del Serial available
}




void setup() {
  Serial.begin(500000);  // LabVIEW

  // Configurar Motores y Frenos
  for (int i = 0; i < NUM_MOTORES; i++) {
    ArrayMotores[i].pwmL = PIN_LPWM[i];
    ArrayMotores[i].pwmR = PIN_RPWM[i];
    pinMode(ArrayMotores[i].pwmL, OUTPUT);
    pinMode(ArrayMotores[i].pwmR, OUTPUT);
    //Inicializar estrcuctura del motor
    ArrayMotores[i].pasosActuales = 0;
    ArrayMotores[i].metaPasos = 0;
    ArrayMotores[i].errorPasos = 0;
    ArrayMotores[i].ultimoError = 0;
    ArrayMotores[i].pasosHomingAux = 0;
    ArrayMotores[i].estadoReporte = ST_IDLE;
    ArrayMotores[i].modoActual = MODE_IDLE;
  }
  pinMode(PIN_FRENO[0], OUTPUT);
  pinMode(PIN_FRENO[1], OUTPUT);

  // Configurar Encoders Hardware
  for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
    pinMode(PIN_ENCODER_A[i], INPUT_PULLUP);
    pinMode(PIN_ENCODER_B[i], INPUT_PULLUP);
    encoderHw[i].portA = portInputRegister(digitalPinToPort(PIN_ENCODER_A[i]));
    encoderHw[i].maskA = digitalPinToBitMask(PIN_ENCODER_A[i]);
    encoderHw[i].portB = portInputRegister(digitalPinToPort(PIN_ENCODER_B[i]));
    encoderHw[i].maskB = digitalPinToBitMask(PIN_ENCODER_B[i]);
  }

  configurarTimer5();
  lastControlUs = micros();
}

void actualizarLogMotor(int i) {
  switch (ArrayMotores[i].modoActual) {
    case MODE_IDLE:
      moverMotor(ArrayMotores[i], 0);  // Freno/Parada
      ArrayMotores[i].estadoReporte = ST_IDLE;
      break;

    case MODE_FAST_MOV:
      {
        int32_t pasos;
        int32_t meta;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
          pasos = ArrayMotores[i].pasosActuales;
          meta = ArrayMotores[i].metaPasos;
        }
        int32_t errorActual = meta - pasos;
        // Lógica de frenado por meta o por paso de signo (igual que ESP32)
        bool yaSePaso = false;
        if (ArrayMotores[i].pwmActual > 0 && errorActual <= 0) yaSePaso = true;
        if (ArrayMotores[i].pwmActual < 0 && errorActual >= 0) yaSePaso = true;

        if (labs(errorActual) < 8 || yaSePaso) {
          moverMotor(ArrayMotores[i], 0);
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
      {
        //bool limiteAlcanzado = leerBit(datosMuestreo.finalesCarrera, i);
        //*****************************************************************SIMULACION
        bool limiteAlcanzado = false;

        if (ArrayMotores[i].ticksRestantes > 0 && !limiteAlcanzado) {
          // Seguimos moviendo el motor a la velocidad indicada
          moverMotor(ArrayMotores[i], ArrayMotores[i].pwmActual);
          ArrayMotores[i].estadoReporte = ST_MOVIENDO;
          ArrayMotores[i].ticksRestantes--;
        } else {
          frenarMotor(ArrayMotores[i]);
          ArrayMotores[i].modoActual = MODE_IDLE;
          ArrayMotores[i].estadoReporte = ST_IDLE;

          if (limiteAlcanzado) {
            ArrayMotores[i].estadoReporte = ST_ERROR_TRABADO;
          } else {
            ArrayMotores[i].estadoReporte = ST_IDLE;
          }
        }
        break;
      }

    case MODE_HOMING:
      // Aquí iría tu lógica de finales de carrera cuando los conectes
      // Por ahora, solo es el cascarón de la máquina de estados
      break;
  }
}

void enviarTelemetriaLabVIEW() {
  MsgDatos_t msg;
  for (int i = 0; i < NUM_ENCODERS; i++) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      msg.pasos[i] = ArrayMotores[i].pasosActuales;
      //*****************************************************************************************************************************SIMULACION
      //msg.pasos[i] = (int32_t)(i * 1000) + random(1000);
    }
  }

  uint16_t sumaCrc = 0;
  sumaCrc += msg.header;
  sumaCrc += msg.funcion;
  sumaCrc += msg.finalesCarrera;
  for (int i = 0; i < 3; i++) sumaCrc += msg.edoMotores[i];

  // Accedemos a los 24  bytes de 'pasos' usando un puntero uint8_t
  uint8_t *pasosBytes = (uint8_t *)msg.pasos;
  for (int i = 0; i < 24; i++) {
    sumaCrc += pasosBytes[i];
  }
  msg.crcFinal = (uint8_t)(sumaCrc & 0xFF);

  Serial.write((uint8_t *)&msg, sizeof(msg));
}


void loop() {
  //Leer comandos de LabVIEW
  procesarLecturaSerial();



  // Ejecutar lógica de control cada 4ms (como en la ESP32)
  if (micros() - lastControlUs >= CONTROL_PERIOD_US) {
    lastControlUs += CONTROL_PERIOD_US;

    for (int i = 0; i < NUM_MOTORES; i++) {
      actualizarLogMotor(i);
    }

    static uint8_t contadorTelemetria = 0;
    contadorTelemetria++;

    if (contadorTelemetria >= 5) {
      enviarTelemetriaLabVIEW();
      contadorTelemetria = 0;
    }
  }
}