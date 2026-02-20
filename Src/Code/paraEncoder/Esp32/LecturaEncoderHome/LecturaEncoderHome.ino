#include <ESP32Encoder.h>

// Pines del Encoder y Home
#define PIN_HOME 4
#define PIN_ENC_A 19
#define PIN_ENC_B 18

// Pines PWM para el Motor
#define PIN_PWM_DER 25
#define PIN_PWM_IZQ 26

// Configuración PWM moderna
const int frecuencia = 5000;
const int resolucion = 8; // 0 a 255

ESP32Encoder encoder;

volatile bool homeDetectado = false;
volatile long pasosCapturados = 0;
volatile unsigned long ultimaInterrupcion = 0;

void IRAM_ATTR funcionInterrupcion() {
  unsigned long tiempoActual = millis();
  if (tiempoActual - ultimaInterrupcion > 200) { 
    pasosCapturados = (long)encoder.getCount();
    encoder.setCount(0); 
    homeDetectado = true;
    ultimaInterrupcion = tiempoActual;
  }
}

void setup() {
  Serial.begin(115200);
  
  // --- NUEVA FORMA DE CONFIGURAR PWM ---
  // Ya no se usan canales (0, 1), se asocia directo al pin
  ledcAttach(PIN_PWM_DER, frecuencia, resolucion);
  ledcAttach(PIN_PWM_IZQ, frecuencia, resolucion);

  // Configuración de Home e Interrupción
 // pinMode(PIN_HOME, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(PIN_HOME), funcionInterrupcion, FALLING);

  // Configuración de Encoder
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(PIN_ENC_A, PIN_ENC_B);
  encoder.clearCount();

  
}

void loop() {
  if (Serial.available() > 0) {
    int velocidad = Serial.parseInt();
    while(Serial.available() > 0) Serial.read(); // Limpiar buffer
    moverMotor(velocidad);
  }

  if (homeDetectado) {
    noInterrupts(); 
    long copiaPasos = pasosCapturados;
    homeDetectado = false;
    interrupts();
    Serial.print("\n>>> ¡HOME! Pasos: ");
    Serial.println(copiaPasos);
  }

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    Serial.print("Posición: ");
    Serial.println((long)encoder.getCount());
    lastPrint = millis();
  }
}

void moverMotor(int v) {
  if (v > 0) {
    ledcWrite(PIN_PWM_DER, abs(v)); // Ahora se usa el PIN directamente
    ledcWrite(PIN_PWM_IZQ, 0);
    Serial.print("DERECHA: "); Serial.println(v);
  } 
  else if (v < 0) {
    ledcWrite(PIN_PWM_DER, 0);
    ledcWrite(PIN_PWM_IZQ, abs(v));
    Serial.print("IZQUIERDA: "); Serial.println(v);
  } 
  else {
    ledcWrite(PIN_PWM_DER, 0);
    ledcWrite(PIN_PWM_IZQ, 0);
    Serial.println("STOP");
  }
}