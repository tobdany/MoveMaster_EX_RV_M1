#include <Arduino.h>
#include <ESP32Encoder.h>

// --- PINES ---
const int PIN_A = 19;
const int PIN_B = 18;
const int PIN_Z = 34; // HOME
const int PIN_PWM_DER = 25; 
const int PIN_PWM_IZQ = 26;

// --- CONFIGURACIÓN ---
ESP32Encoder encoder;
const int frecuencia = 5000;
const int resolucion = 8;
bool homeEncontrado = false;

void setup() {
    Serial.begin(115200);
    
    // Configuración Motor (PWM)
    ledcAttach(PIN_PWM_DER, frecuencia, resolucion);
    ledcAttach(PIN_PWM_IZQ, frecuencia, resolucion);
    
    // Configuración Encoder
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(PIN_A, PIN_B);
    encoder.clearCount();
    
    // Configuración Sensor Home
    pinMode(PIN_Z, INPUT_PULLUP);

    Serial.println("--- BUSCADOR DE HOME INICIADO ---");
    Serial.println("Escribe 'go' para iniciar la busqueda a PWM 25.");
}

void loop() {
    // 1. Escuchar comando para iniciar
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.equalsIgnoreCase("go")) {
            homeEncontrado = false;
            encoder.clearCount();
            Serial.println("[MOTORES] -> Buscando sensor...");
            // Arrancamos el motor a la derecha (ajusta si es a la izquierda)
            ledcWrite(PIN_PWM_DER, 25);
            ledcWrite(PIN_PWM_IZQ, 0);
        }
    }

    // 2. Monitoreo constante del sensor
    if (!homeEncontrado) {
        if (digitalRead(PIN_Z) == LOW) {
            // ¡LO ENCONTRAMOS! Detenemos todo de inmediato
            ledcWrite(PIN_PWM_DER, 0);
            ledcWrite(PIN_PWM_IZQ, 0);
            
            long pasosFinales = (long)encoder.getCount();
            encoder.clearCount(); // Establecemos el 0 real aquí
            homeEncontrado = true;

            Serial.println("\n******************************");
            Serial.println("¡HOME DETECTADO!");
            Serial.print("Pasos recorridos hasta el sensor: ");
            Serial.println(pasosFinales);
            Serial.println("Contador reseteado a 0.");
            Serial.println("******************************");
        } else {
            // Opcional: imprimir pasos mientras busca (cada cierto tiempo)
            static unsigned long lastPrint = 0;
            if (millis() - lastPrint > 100 && !homeEncontrado && digitalRead(PIN_PWM_DER) > 0) {
                Serial.print("Buscando... Pasos: ");
                Serial.println((long)encoder.getCount());
                lastPrint = millis();
            }
        }
    }
}