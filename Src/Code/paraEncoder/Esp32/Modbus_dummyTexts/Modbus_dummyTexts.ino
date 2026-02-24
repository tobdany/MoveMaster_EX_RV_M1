#include <Arduino.h>

const int PASOS_POR_VUELTA = 400;
int32_t pasosSimulados = 0;
uint8_t idSimulado = 0;

const int NUM_MOTORES = 6;
const int BYTES_POR_MOTOR = 10; //4 pasos, 2 vueltas, 2 velocidad, 2 de reserva o espacio

void setup() {
  Serial.begin(115200);
}

void loop() {
  //enviarTelemetriaGlobal();
 // delay(10); 
  telemetriaPorEncoder();
  delay(10); 
}


/**
 * @brief  Empaqueta los datos de todos los motores en un solo envío atómico de 63 bytes. Al enviar todos los ejes juntos, 
 * se garantiza que LabVIEW reciba una "fotografía" exacta de la posición del brazo 
 * en un instante de tiempo (determinismo).
 * * @struct Trama_Global (63 bytes):
 * [0]      - Header: 0x3A (':')
 * [1]      - Función: 0x02 (Global Telemetry)
 * [2]      - Byte Count: 0x3C (60 bytes de datos)
 * [3-62]   - Bloques de Motor (6 motores x 10 bytes cada uno):
 * + Offset 0-3: Pasos (int32_t) - Posición absoluta del encoder.
 * + Offset 4-5: Vueltas (int16_t) - Contador de revoluciones.
 * + Offset 6-7: Velocidad (int16_t) - Velocidad instantánea en RPM o pasos/s.
 * + Offset 8-9: Bytes de reserva
 * [63]     - Checksum: uint8_t (Suma de bytes Función + Datos) & 0xFF.
 * * @return void
 * @note   El cálculo de 'offset' usa (3 + i * 10) para respetar los bytes de control [0-2].
 * @warning Asegurarse de que el buffer Serial de LabVIEW esté configurado para al menos 126 bytes 
 * para evitar desbordamientos en altas frecuencias (100Hz+).
 */

void enviarTelemetriaGlobal() {
  // Estructura: [:] [F][BC] [DATOS_M0...M5] [CRC]
  //Datos por motor: 4 pasos, 2 vueltas, 2 velocidad, 2 de reserva
  // Total: 1 + 1 + (6 * 10) + 1 = 63 bytes
  const uint8_t bufferSize=64;
  uint8_t frame[bufferSize];
  uint16_t sumaCrc = 0;

  frame[0] = 0x3A; // Cabecera ':'
  frame[1] = 0x02; // Función: Lectura Global
  frame[2] = bufferSize-3-1;
  sumaCrc += frame[1] + frame[2];

  for (int i = 0; i < NUM_MOTORES; i++) {
    // Simulamos datos para cada motor
    int32_t p = (i * 1000); // + (millis() / 10); // Pasos variando
    int16_t v = (int16_t)i;                  // Vueltas = ID
    int16_t vel = 100 + i;                   // Velocidad

    int offset = 3 + (i * BYTES_POR_MOTOR);
    
    memcpy(&frame[offset], &p, 4);           // Byte offset a offset+3
    memcpy(&frame[offset + 4], &v, 2);       // Byte offset+4 a offset+5
    memcpy(&frame[offset + 6], &vel, 2);     // Byte offset+6 a offset+7
    // Bytes offset+8 y offset+9 quedan en 0 (Reserva)
    frame[offset + 8] = 0;
    frame[offset + 9] = 0;

    // Sumamos todos los bytes del motor al CRC
    for (int j = 0; j < BYTES_POR_MOTOR; j++) {
      sumaCrc += frame[offset + j];
    }
  }

  frame[bufferSize-1] = (uint8_t)(sumaCrc & 0xFF);
  Serial.write(frame, bufferSize);
}

/**
 * @brief  Transmite la telemetría individual de un eje específico (15 bytes).
 * * @struct Trama_Individual (14 bytes total):
 * [0]    - Header: 0x3A (':')
 * [1]    - Función: 0x01 (Telemetría individual)
 * [2]    - Longitud: 0x0B (11 bytes: ID + 10 bytes de datos de motor)
 * [3]    - ID: Identificador del motor (0-5)
 * [4-7]  - Pasos: int32_t (4 bytes)
 * [8-9]  - Vueltas: int16_t (2 bytes)
 * [10-11]- Velocidad: int16_t (2 bytes)
 * [12-13]- Reserva: 0x00 (2 bytes de espacio) <--- Agregados para paridad con Global
 * [14]   - Checksum: (Function + Length + ID + Data) & 0xFF
 */
void telemetriaPorEncoder() {
  pasosSimulados += 50; 
  if (pasosSimulados > 4000) pasosSimulados = 0;
  idSimulado = (idSimulado + 1) % NUM_MOTORES;

  int16_t vueltas = (int16_t)(pasosSimulados / PASOS_POR_VUELTA);
  int16_t vel = 120; 
  
  uint8_t f = 0x01;         // Función
  uint8_t payloadSize = 11; // ID(1) + BloqueMotor(10) = 11 bytes
  
  // Creamos un buffer para el bloque de datos del motor (10 bytes como en la global)
  uint8_t motorData[10];
  memcpy(&motorData[0], &pasosSimulados, 4);
  memcpy(&motorData[4], &vueltas, 2);
  memcpy(&motorData[6], &vel, 2);
  motorData[8] = 0; // Reserva 1
  motorData[9] = 0; // Reserva 2

  // Cálculo del Checksum (Función + Longitud + ID + Datos)
  uint16_t sumaCrc = f + payloadSize + idSimulado;
  for(int i = 0; i < 10; i++) {
    sumaCrc += motorData[i];
  }

  // Envío de la trama completa (Total 15 bytes contando el CRC final)
  Serial.write(0x3A);         // [0] Header ':'
  Serial.write(f);            // [1] Función
  Serial.write(payloadSize);  // [2] Longitud de datos
  Serial.write(idSimulado);   // [3] ID del motor
  Serial.write(motorData, 10);// [4-13] Datos (incluye los 2 de reserva)
  Serial.write((uint8_t)(sumaCrc & 0xFF)); // [14] Checksum
}