#include <Arduino.h>

const int PASOS_POR_VUELTA = 400;
int32_t pasosSimulados = 0;
uint8_t idSimulado = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  pasosSimulados += 50; 
  if (pasosSimulados > 400) pasosSimulados = 0;
  
  idSimulado++;
  if (idSimulado > 4) idSimulado = 0;

  enviarTelemetriaPrueba(idSimulado, pasosSimulados);

  delay(50); 
}

void enviarTelemetriaPrueba(uint8_t id, int32_t p) {
    int16_t vueltas = (int16_t)(p / PASOS_POR_VUELTA);
    int16_t vel = 100; 
    uint8_t f = 0x02;
    
    // 1. Empaquetamos los datos exactamente como se enviarán
    uint8_t data[8];
    memcpy(&data[0], &p, 4);       // Pasos (4 bytes)
    memcpy(&data[4], &vueltas, 2); // Vueltas (2 bytes)
    memcpy(&data[6], &vel, 2);     // Velocidad (2 bytes)

    // 2. Calculamos el CRC sumando byte por byte
    uint16_t sumaCrc = f + id;
    for(int i = 0; i < 8; i++) {
        sumaCrc += data[i];
    }
    uint8_t crcFinal = (uint8_t)(sumaCrc & 0xFF);

    // 3. Envío de la trama de 12 bytes
    Serial.write(0x3A);      // 0: Header
    Serial.write(f);         // 1: Función
    Serial.write(id);        // 2: ID
    Serial.write(data, 8);   // 3-10: Datos (Pasos, Vueltas, Vel)
    Serial.write(crcFinal);  // 11: Checksum
}