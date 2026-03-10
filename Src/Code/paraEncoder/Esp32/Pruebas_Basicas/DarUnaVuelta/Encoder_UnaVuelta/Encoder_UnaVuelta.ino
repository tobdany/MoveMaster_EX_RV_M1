#include <Arduino.h>
#include <ESP32Encoder.h>

// --- CONFIGURACIÓN ---
const int PIN_A = 19;
const int PIN_B = 18;
const int PIN_Z = 34;
const int PIN_PWM_DER = 25; 
const int PIN_PWM_IZQ = 26;

const int PASOS_OBJETIVO = 400; // 200 PPR * 2 (HalfQuad)
const int PWM_VELOCIDAD = 35;   // Un poco más que 25 para vencer la inercia inicial

ESP32Encoder encoder;
bool moviendo = false;

void setup() {
    Serial.begin(115200);
    
    ledcAttach(PIN_PWM_DER, 5000, 8);
    ledcAttach(PIN_PWM_IZQ, 5000, 8);
    
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(PIN_A, PIN_B);
    encoder.clearCount();
    
    pinMode(PIN_Z, INPUT_PULLUP);

    Serial.println("--- CONTROL DE POSICIÓN ---");
    Serial.println("Comandos: 'go' (busca home), 'vuelta' (da 1 vuelta), 'clear' (reset)");
}

void loop() {
    // 1. Lectura de Comandos
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd.equalsIgnoreCase("go")) {
            Serial.println("[!] Buscando Home...");
            encoder.clearCount();
            moviendo = true;
            ledcWrite(PIN_PWM_DER, 25); 
            ledcWrite(PIN_PWM_IZQ, 0);
        }
        
        else if (cmd.equalsIgnoreCase("vuelta")) {
            Serial.println("[!] Ejecutando 1 vuelta completa (400 pasos)...");
            encoder.clearCount(); // Empezamos desde 0 para la vuelta
            moviendo = true;
            ledcWrite(PIN_PWM_DER, PWM_VELOCIDAD);
            ledcWrite(PIN_PWM_IZQ, 0);
        }

        else if (cmd.equalsIgnoreCase("clear")) {
            encoder.clearCount();
            Serial.println("[SISTEMA] -> Contador a 0.");
        }
    }

    // 2. Control de Movimiento
    if (moviendo) {
        long pasosActuales = (long)encoder.getCount();

        // Si estamos buscando HOME (comando 'go')
       
        // Si estamos dando una VUELTA (comando 'vuelta')
        if (pasosActuales >= PASOS_OBJETIVO) {
            frenarMotor("VUELTA COMPLETADA");
        }

        // Monitor de progreso cada 50 pasos
        static long ultimoReporte = 0;
        if (abs(pasosActuales - ultimoReporte) >= 50) {
            Serial.print("Progreso: ");
            Serial.println(pasosActuales);
            ultimoReporte = pasosActuales;
        }
    }
}

void frenarMotor(String motivo) {
    // Freno rápido: apagamos ambos
    ledcWrite(PIN_PWM_DER, 0);
    ledcWrite(PIN_PWM_IZQ, 0);
    
    moviendo = false;
    
    Serial.println("\n------------------------------");
    Serial.print("DETENIDO: "); Serial.println(motivo);
    Serial.print("Posición final: "); Serial.println((long)encoder.getCount());
    Serial.println("------------------------------");
}