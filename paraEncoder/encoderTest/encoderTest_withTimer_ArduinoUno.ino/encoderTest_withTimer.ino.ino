#include <TimerOne.h>

const int L_PWM = 6;
const int R_PWM = 5;
const int pinA = 2; 
const int pinB = 7; 
const int pinZ = 3; 

volatile long motorPosition = 0;
volatile long pasosFinales = 0; // Variable para capturar el resultado
volatile bool nuevaVueltaDisponible = false;
volatile int homeSignal = 0;
volatile byte lastEncoderState = 0;

enum Estado { PARADO, ADELANTE, ATRAS, BUSCANDO_HOME, UNA_VUELTA };
volatile Estado modoActual = PARADO;

const int8_t encoderTable[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void setup() {
  Serial.begin(115200); // Sugiero subir a 500k para mayor fluidez
  
  pinMode(L_PWM, OUTPUT);
  pinMode(R_PWM, OUTPUT);
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  pinMode(pinZ, INPUT_PULLUP);

  byte a = digitalRead(pinA);
  byte b = digitalRead(pinB);
  lastEncoderState = (a << 1) | b;

  Timer1.initialize(40); 
  Timer1.attachInterrupt(motorControlISR);

  frenoMotor();
}

void motorControlISR() {
  // 1. Decodificación de Cuadratura
  byte a = digitalRead(pinA);
  byte b = digitalRead(pinB);
  byte currentState = (a << 1) | b;
  byte index = (lastEncoderState << 2) | currentState;
  motorPosition += encoderTable[index];
  lastEncoderState = currentState;

  // 2. Control de Home
  if (digitalRead(pinZ) == LOW) {
    homeSignal = 500;
    if (modoActual == BUSCANDO_HOME || modoActual == UNA_VUELTA) {
      // CAPTURA: Guardamos el valor antes de resetear
      pasosFinales = motorPosition;
      nuevaVueltaDisponible = true;

      // Freno de seguridad
      analogWrite(R_PWM, 0);
      analogWrite(L_PWM, 0);
      digitalWrite(R_PWM, LOW);
      digitalWrite(L_PWM, LOW);
      
      modoActual = PARADO;
      motorPosition = 0; 
    }
  }
}

void loop() {
  // Reporte de pasos (Se activa cuando la ISR detecta el fin de la vuelta)
  if (nuevaVueltaDisponible) {
    Serial.print("\n>>> VUELTA COMPLETADA. PASOS: ");
    Serial.println(pasosFinales);
    nuevaVueltaDisponible = false;
  }

  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'f') { modoActual = ADELANTE; moveForward(); }
    if (c == 'b') { modoActual = ATRAS; moveBackward(); }
    if (c == 's') { modoActual = PARADO; frenoMotor(); }
    if (c == 'h') { motorPosition = 0; modoActual = BUSCANDO_HOME; moveForward(); }
    if (c == 'v') { 
      motorPosition = 0;
      modoActual = UNA_VUELTA; 
      moveForward(); 
      delay(1); 
    }
  }

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 100) {
    Serial.print("P:"); Serial.print(motorPosition);
    Serial.print(",H:"); Serial.println(homeSignal);
    homeSignal = 0; 
    lastPrint = millis();
  }
}

void moveForward() { analogWrite(R_PWM, 50); analogWrite(L_PWM, 0); } // Velocidad sugerida 40
void moveBackward() { analogWrite(R_PWM, 0); analogWrite(L_PWM, 50); }
void frenoMotor() {
  analogWrite(R_PWM, 0);
  analogWrite(L_PWM, 0);
  digitalWrite(R_PWM, LOW);
  digitalWrite(L_PWM, LOW);
}