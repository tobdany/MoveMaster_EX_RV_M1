#include <TimerOne.h>

const int L_PWM = 6;
const int R_PWM = 5;
const int pinA = 2; // PD2
const int pinB = 7; // PD7
const int pinZ = 3; // PD3

volatile long motorPosition = 0;
volatile long pasosFinales = 0;
volatile bool nuevaVueltaDisponible = false;
volatile byte lastEncoderState = 0;

enum Estado { PARADO, ADELANTE, ATRAS, BUSCANDO_HOME, UNA_VUELTA };
volatile Estado modoActual = PARADO;

// Tabla de búsqueda optimizada
const int8_t encoderTable[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void setup() {
  Serial.begin(115200); 
  
  pinMode(L_PWM, OUTPUT);
  pinMode(R_PWM, OUTPUT);
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  pinMode(pinZ, INPUT_PULLUP);

  // Inicializar estado leyendo directamente los pines
  lastEncoderState = ((PIND & (1 << PD2)) ? 2 : 0) | ((PIND & (1 << PD7)) ? 1 : 0);

  // Muestreo a 20us (50kHz)
  Timer1.initialize(10); 
  Timer1.attachInterrupt(motorControlISR);

  frenoMotor();
}

void motorControlISR() {
  // LECTURA DIRECTA DE REGISTROS (Mucho más rápido que digitalRead)
  // Pin 2 es PD2, Pin 7 es PD7
  byte a = (PIND & (1 << PD2)) ? 1 : 0;
  byte b = (PIND & (1 << PD7)) ? 1 : 0;
  
  byte currentState = (a << 1) | b;
  byte index = (lastEncoderState << 2) | currentState;
  
  // Invertimos el signo aquí para que 'f' de pasos positivos
  motorPosition -= encoderTable[index]; 
  lastEncoderState = currentState;

  // Detección de Home (Pin 3 es PD3)
  if (!(PIND & (1 << PD3))) { 
    if (modoActual == BUSCANDO_HOME || modoActual == UNA_VUELTA) {
      pasosFinales = motorPosition;
      nuevaVueltaDisponible = true;
      
      // Freno de hardware inmediato
      OCR0A = 0; // PWM pin 6 a 0
      OCR0B = 0; // PWM pin 5 a 0
      PORTD &= ~(1 << PD5); 
      PORTD &= ~(1 << PD6);
      
      modoActual = PARADO;
      motorPosition = 0; 
    }
  }
}

void loop() {
  if (nuevaVueltaDisponible) {
    Serial.print("V:"); Serial.println(pasosFinales);
    nuevaVueltaDisponible = false;
  }

  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'f') { modoActual = ADELANTE; moveForward(255); }
    if (c == 'b') { modoActual = ATRAS; moveBackward(255); }
    if (c == 's') { modoActual = PARADO; frenoMotor(); }
    if (c == 'h') { motorPosition = 0; modoActual = BUSCANDO_HOME; moveForward(100); }
    if (c == 'v') { 
      motorPosition = 0;
      modoActual = UNA_VUELTA; 
      moveForward(35); 
      //delay(1); 
    }
  }
}

void moveForward(int v) { analogWrite(R_PWM, v); analogWrite(L_PWM, 0); }
void moveBackward(int v) { analogWrite(R_PWM, 0); analogWrite(L_PWM, v); }
void frenoMotor() {
  analogWrite(R_PWM, 0); analogWrite(L_PWM, 0);
  digitalWrite(R_PWM, LOW); digitalWrite(L_PWM, LOW);
}