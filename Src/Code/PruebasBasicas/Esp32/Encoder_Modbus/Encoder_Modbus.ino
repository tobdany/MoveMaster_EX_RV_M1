#include <ESP32Encoder.h>

// --- CONFIGURACIÓN DE 6 ENCODERS ---
const int NUM_ENCODERS = 6;
ESP32Encoder encoders[NUM_ENCODERS];

// Definición de pines (ajusta según tu conexión real)
// Formato: {PinA, PinB}
int pinesEncoder[NUM_ENCODERS][2] = {
  {19, 18}, // ID 0
  {5,  17}, // ID 1
  {16, 4},  // ID 2
  {0,  2},  // ID 3 (Cuidado con el pin 0 en el arranque)
  {15, 13}, // ID 4
  {12, 14}  // ID 5
};

const int PASOS_POR_VUELTA = 400;

// Variables para cálculos de velocidad (6 ejes)
long lastPos[NUM_ENCODERS] = {0};
int16_t velocidades[NUM_ENCODERS] = {0};
unsigned long lastVelTime = 0;

void setup() {
    Serial.begin(115200);

    // Inicializar los 6 encoders por hardware (PCNT)
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    for(int i = 0; i < NUM_ENCODERS; i++) {
        encoders[i].attachHalfQuad(pinesEncoder[i][0], pinesEncoder[i][1]);
        encoders[i].clearCount();
    }
}

void loop() {
    // 1. Calcular velocidad de todos los ejes cada 10ms
    unsigned long timeNow = millis();
    if (timeNow - lastVelTime >= 10) {
        for(int i = 0; i < NUM_ENCODERS; i++) {
            long currentPos = (long)encoders[i].getCount();
            velocidades[i] = (int16_t)(currentPos - lastPos[i]);
            lastPos[i] = currentPos;
        }
        lastVelTime = timeNow;
    }

    // 2. Procesar Comandos Modbus
    if (Serial.available() >= 5) {
        if (Serial.read() == 0x3A) { 
            uint8_t func = Serial.read();
            int16_t val = (Serial.read() << 8) | Serial.read(); // 'val' puede ser el ID o la velocidad
            uint8_t crcRecibido = Serial.read();
            uint8_t crcCalculado = (func + (val >> 8) + (val & 0xFF)) & 0xFF;

            if (crcCalculado == crcRecibido) {
                ejecutarComando(func, val);
            }
        }
    }
}

void ejecutarComando(uint8_t func, int16_t val) {
    switch (func) {
        case 0x02: // LEER: Telemetría de un encoder específico
            // Aquí 'val' será el ID del encoder (0 a 5) enviado desde LabVIEW
            if (val >= 0 && val < NUM_ENCODERS) {
                enviarTelemetriaBinaria(val);
            }
            break;
            
        // Otros casos (velocidad, etc) requerirían un protocolo de escritura
        // que incluya el ID del motor.
    }
}

void enviarTelemetriaBinaria(uint8_t id) {
    int32_t pasos = (int32_t)encoders[id].getCount();
    int16_t vueltas = (int16_t)(pasos / PASOS_POR_VUELTA);
    int16_t vel = velocidades[id];
    uint8_t f = 0x02;

    // 1. Creamos un buffer temporal para los datos que varían (Pasos, Vueltas, Vel)
    // Esto nos permite sumar los bytes uno por uno
    uint8_t data[8];
    memcpy(&data[0], &pasos, 4);   // Bytes de pasos (Little Endian)
    memcpy(&data[4], &vueltas, 2); // Bytes de vueltas
    memcpy(&data[6], &vel, 2);     // Bytes de velocidad

    // 2. Calculamos el CRC sumando CADA byte que se va a enviar
    // Iniciamos con la función y el ID
    uint16_t sumaCrc = f + id; 

    // Sumamos los 8 bytes de datos (pasos + vueltas + vel)
    for(int i = 0; i < 8; i++) {
        sumaCrc += data[i];
    }

    // El CRC final es el residuo de 8 bits (Módulo 256)
    uint8_t crcFinal = (uint8_t)(sumaCrc & 0xFF);

    // 3. ENVIAR TRAMA COMPLETA (12 Bytes en total)
    Serial.write(0x3A);      // Byte 0: Cabecera
    Serial.write(f);         // Byte 1: Función
    Serial.write(id);        // Byte 2: ID
    Serial.write(data, 8);   // Bytes 3 al 10: Datos
    Serial.write(crcFinal);  // Byte 11: Checksum
}