#include <Arduino.h>
#include <ESP32Encoder.h>

// --- CONFIGURACIÓN DE PINES ---
const int PIN_A = 19;
const int PIN_B = 18;
const int PIN_Z = 34; // Tu pin de HOME / Indice Z

ESP32Encoder encoder;
long posicionAnterior = 0;

void setup() {
    Serial.begin(115200);
    
    // Configuración física del Encoder
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(PIN_A, PIN_B);
    encoder.clearCount();
    
    // Configuración del Pin Z (HOME)
    pinMode(PIN_Z, INPUT_PULLUP);

    Serial.println("--- DIAGNÓSTICO DE ENCODER ACTIVO ---");
    Serial.println("Escribe 'clear' para reiniciar el conteo.");
}

void loop() {
    // 1. Lógica para limpiar (Reset)
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim(); // Quita espacios o saltos de línea extra
        
        if (cmd.equalsIgnoreCase("clear")) {
            encoder.clearCount();
            posicionAnterior = 0;
            Serial.println("\n[SISTEMA] -> Contador reiniciado a 0.");
        }
    }

    // 2. Lectura y detección de dirección
    long posicionActual = (long)encoder.getCount();
    
    // Solo imprimimos si el valor cambia para no saturar el Serial
    if (posicionActual != posicionAnterior) {
        
        // Determinamos dirección basándonos en el cambio de pasos
        String dir = (posicionActual > posicionAnterior) ? "DER >>" : "<< IZQ";
        
        Serial.print("Pasos: ");
        Serial.print(posicionActual);
        Serial.print(" | ");
        Serial.print(dir);
        
        // 3. Monitoreo del Pin Z (Home)
        if (digitalRead(PIN_Z) == LOW) {
            Serial.print(" | [!] PIN Z DETECTADO");
        }
        
        Serial.println();
        posicionAnterior = posicionActual;
    }
    
    delay(5); // Estabilidad para el procesador
}