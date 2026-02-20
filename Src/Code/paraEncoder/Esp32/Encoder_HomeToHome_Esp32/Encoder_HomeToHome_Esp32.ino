#include <ESP32Encoder.h>

// Pines del Encoder y Home
#define PIN_HOME 4
#define PIN_ENC_A 19
#define PIN_ENC_B 18
#define PIN_PWM_DER 25
#define PIN_PWM_IZQ 26

const int frecuencia = 5000;
const int resolucion = 8; 

ESP32Encoder encoder;

volatile bool homeDetectado = false;
volatile long pasosCapturados = 0;
volatile unsigned long ultimaInterrupcion = 0;

// Estado del sistema
bool buscandoHome = false;
const int VELOCIDAD_BUSQUEDA = 120; // Velocidad para la vuelta

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
  
  ledcAttach(PIN_PWM_DER, frecuencia, resolucion);
  ledcAttach(PIN_PWM_IZQ, frecuencia, resolucion);

  pinMode(PIN_HOME, INPUT_PULLUP);
  // Reactivamos la interrupción
  attachInterrupt(digitalPinToInterrupt(PIN_HOME), funcionInterrupcion, FALLING);

  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(PIN_ENC_A, PIN_ENC_B);
  encoder.clearCount();

  //Serial.println(">>> Sistema Listo. Envíe 'v' para dar una vuelta completa.");
}

void loop() {
  // 1. Lectura de comandos Serial
  if (Serial.available() > 0) {
    char comando = Serial.read(); // Leemos un caracter
    
    if (comando == 'v' || comando == 'V') {
      Serial.println(">>> Saliendo de zona de Home...");
      buscandoHome = false; // Aún no buscamos el final
      moverMotor(VELOCIDAD_BUSQUEDA);

      // Esperar activamente a que el sensor se libere (el brazo salga del sensor)
      // Mientras el pin esté en LOW (detectando), nos quedamos aquí
      while(digitalRead(PIN_HOME) == LOW) {
        delay(10); // Pequeña espera para no saturar
      }
      
      Serial.println(">>> Sensor liberado. Buscando siguiente Home...");
      delay(100); // Un margen extra de seguridad
      homeDetectado = false; // Limpiamos cualquier disparo falso al salir
      buscandoHome = true;  // Ahora sí, activamos la vigilancia para el paro
    }
  }

  // 2. Lógica de detención al llegar al Home
  if (buscandoHome && homeDetectado) {
    moverMotor(0); // ¡Detener inmediatamente!
    buscandoHome = false;
    
    noInterrupts(); 
    long copiaPasos = pasosCapturados;
    homeDetectado = false;
    interrupts();

    Serial.print("\n>>> ¡LLEGADA A HOME!");
    Serial.print(" Pasos totales de la vuelta: ");
    Serial.println(copiaPasos);
  }

  // 3. Monitor de posición (solo imprime si no estamos buscando home para no saturar)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 250) {
    Serial.print("Posición actual: ");
    Serial.println((long)encoder.getCount());
    lastPrint = millis();
  }
}

void moverMotor(int v) {
  if (v > 0) {
    ledcWrite(PIN_PWM_DER, abs(v));
    ledcWrite(PIN_PWM_IZQ, 0);
  } 
  else if (v < 0) {
    ledcWrite(PIN_PWM_DER, 0);
    ledcWrite(PIN_PWM_IZQ, abs(v));
  } 
  else {
    ledcWrite(PIN_PWM_DER, 0);
    ledcWrite(PIN_PWM_IZQ, 0);
  }
}