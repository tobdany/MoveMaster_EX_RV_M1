#include <ESP32Encoder.h>

#define PIN_HOME 4

ESP32Encoder encoder;

// Variables volátiles para comunicación entre la interrupción y el loop
volatile bool homeDetectado = false;
volatile long pasosCapturados = 0;

// Esta es la función que se ejecuta instantáneamente al tocar el sensor
void IRAM_ATTR funcionInterrupcion() {
  pasosCapturados = (long)encoder.getCount();
 // encoder.setCount(0); // Reiniciamos el contador de inmediato
  homeDetectado = true;
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_HOME, INPUT_PULLUP);

  // Configuramos la interrupción
  // FALLING significa que se activa cuando el pin pasa de HIGH a LOW (contacto a GND)
  attachInterrupt(digitalPinToInterrupt(PIN_HOME), funcionInterrupcion, FALLING);

  encoder.attachHalfQuad(19, 18);
  encoder.clearCount();

  Serial.println("Sistema listo. Esperando primer paso por HOME...");
}

void loop() {
  // Solo imprimimos cuando la interrupción nos avisa que detectó algo
  if (homeDetectado) {
    Serial.print(">>> ¡HOME! Pasos en esta revolución: ");
    Serial.println(pasosCapturados);
    
    homeDetectado = false; // Resetear la bandera
  }

  // Monitor opcional cada 500ms para ver que todo se mueva
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 100) {
    Serial.print("Contando: ");
    Serial.println((long)encoder.getCount());
    lastPrint = millis();
  }
}