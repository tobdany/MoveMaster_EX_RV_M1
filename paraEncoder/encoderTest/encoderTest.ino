const int L_PWM = 6;
const int R_PWM = 5;
const int encoderPinA = 2; 
const int encoderPinB = 7; 
const int encoderPinZ = 3; 

volatile long motorPosition = 0;
volatile int homeSignal = 0;
bool lastA = LOW;

enum Estado { PARADO, ADELANTE, ATRAS, BUSCANDO_HOME, UNA_VUELTA };
Estado modoActual = PARADO;

void setup() {
  Serial.begin(115200); 
  
  pinMode(L_PWM, OUTPUT);
  pinMode(R_PWM, OUTPUT);
  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);
  pinMode(encoderPinZ, INPUT_PULLUP);

  lastA = digitalRead(encoderPinA);
  stopMotor(); // Ahora sí está declarada abajo
}

void loop() {
  // 1. LECTURA DEL ENCODER
  bool currentA = digitalRead(encoderPinA);
  if (currentA != lastA) {
    if (digitalRead(encoderPinB) != currentA) motorPosition--;
    else motorPosition++;
    lastA = currentA;
  }

  // 2. DETECCIÓN DE HOME
  if (digitalRead(encoderPinZ) == LOW) {
    if (modoActual == BUSCANDO_HOME || modoActual == UNA_VUELTA) {
      stopMotor();
      if (modoActual == UNA_VUELTA) {
        Serial.print("\n>>> PASOS TOTALES: ");
        Serial.println(motorPosition);
      }
      modoActual = PARADO;
    }
    homeSignal = 500; 
  }

  // 3. COMANDOS
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'f') { modoActual = ADELANTE; moveForward(); }
    if (c == 'b') { modoActual = ATRAS; moveBackward(); }
    if (c == 's') { modoActual = PARADO; stopMotor(); }
    if (c == 'h') { motorPosition = 0; modoActual = BUSCANDO_HOME; moveForward(); }
    if (c == 'v') { 
      motorPosition = 0; 
      modoActual = UNA_VUELTA;
      moveForward();
      delay(200); 
    }
  }

  // 4. GRAFICADO
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 80) {
   /* Serial.print("P:"); Serial.print(motorPosition);
    Serial.print(",H:"); Serial.println(homeSignal);*/
    homeSignal = 0; 
    lastPrint = millis();
  }
}

// --- FUNCIONES DE MOVIMIENTO ---

void moveForward() { 
  analogWrite(R_PWM, 50); 
  analogWrite(L_PWM, 0); 
}

void moveBackward() { 
  analogWrite(R_PWM, 0); 
  analogWrite(L_PWM, 50); 
}

void stopMotor() {
  // 1. Detenemos el PWM (Timer)
  analogWrite(R_PWM, 0);
  analogWrite(L_PWM, 0);
  // 2. Forzamos estado LOW (Físico) para evitar el modo "BURN" de tu tabla
  digitalWrite(R_PWM, LOW);
  digitalWrite(L_PWM, LOW);
}