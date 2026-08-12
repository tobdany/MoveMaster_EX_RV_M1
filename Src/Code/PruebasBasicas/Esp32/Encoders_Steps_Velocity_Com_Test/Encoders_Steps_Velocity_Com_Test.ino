/**
 * @file Encoders_Steps_Velocity_Com_Test.ino
 * @version 1.1 (Etapa de Testeo - Sincronización de Fase)
 * @section DESCRIPCIÓN
 * Sistema de control para 1 motor y sensor de referencia física (HOME). 
 * * El sistema implementa una lógica de "Sincronización de Fase Absoluta":
 * Al recibir un comando de movimiento (1V o 10V), el motor primero busca el 
 * inicio físico de la muesca del sensor, resetea el contador de pasos a 0 en 
 * ese instante exacto, y comienza el conteo de vueltas reales.
 * * @section PROTOCOLO_COMUNICACION (Serial 115200 baud)
 * Comandos recibidos (ASCII + \n):
 * - "HOME:INIT:[PWM]" : Busca el sensor, frena y establece el Cero Real.
 * - "MOV:1V:[PWM]"   : Busca el sensor, sincroniza a 0 y da 1 vuelta completa.
 * - "MOV:10V:[PWM]"  : Busca el sensor, sincroniza a 0 y da 10 vueltas completas.
 * * Trama de Telemetría enviada (Binaria - 15 bytes):
 * [0]   Header (':') -> 0x3A
 * [1]   ID Función   -> 0x01
 * [2]   Longitud     -> 11
 * [3]   ID Motor     -> 0x00
 * [4-7] Pasos (int32) -> Posición relativa al último Home
 * [8-9] Vueltas (int16)-> Contador de pulsos físicos del sensor
 * [10-11] Vel (int16) -> Delta de pasos por cada 10ms
 * [12]  Estado       -> 0:IDLE, 1:HOME, 2:1V, 3:10V
 * [13]  PWM Actual   -> Valor 0-255 enviado al motor
 * [14]  Checksum     -> Suma simple de bytes [1] al [13]
 * * @section NOTAS_TESTEO
 * - El sensor de HOME presenta un "ancho físico" (se mantiene activo varios pasos).
 * - Se implementa un "Candado Lógico" (sensorActivado) para evitar conteos falsos
 * mientras el motor permanece sobre la muesca al iniciar el movimiento.
 */


#include <Arduino.h>
#include <ESP32Encoder.h>

// --- CONFIGURACIÓN HARDWARE ---
const int NUM_MOTORES = 1;
ESP32Encoder encoders[NUM_MOTORES];
int pinesEncoder[NUM_MOTORES][2] = {{19, 18}};
const int PIN_PWM_DER = 25; 
const int PIN_PWM_IZQ = 26;
const int PIN_HOME = 34; 

const int frecuencia = 5000;
const int resolucion = 8;
const int PASOS_POR_VUELTA = 400;

// --- VARIABLES DE CONTROL ---
long lastPos[NUM_MOTORES] = {0};
int16_t velocidades[NUM_MOTORES] = {0};
uint16_t pwmActual[NUM_MOTORES] = {0};

int cont_vueltas_objetivo = 0;
int vueltas_actuales = 0;
bool sensorActivado = false; 
bool movimientoPorVueltas = false;
bool buscandoHomeFisico = false;
uint8_t estadoActual = 0; 

unsigned long lastVelTime = 0;

void setup() {
    Serial.begin(115200);
    ledcAttach(PIN_PWM_DER, frecuencia, resolucion);
    ledcAttach(PIN_PWM_IZQ, frecuencia, resolucion);
    pinMode(PIN_HOME, INPUT_PULLUP);
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoders[0].attachHalfQuad(pinesEncoder[0][0], pinesEncoder[0][1]);
    encoders[0].clearCount();
}

void loop() {
    unsigned long timeNow = millis();

    if (timeNow - lastVelTime >= 10) {
        long currentPos = (long)encoders[0].getCount();
        velocidades[0] = (int16_t)(currentPos - lastPos[0]);
        lastPos[0] = currentPos;

        bool lecturaHome = (digitalRead(PIN_HOME) == LOW);

        // LÓGICA DE CONTEO CON CANDADO (Sensor Ancho)
        if (lecturaHome && !sensorActivado) {
            vueltas_actuales++;
            sensorActivado = true; 
        } 
        if (!lecturaHome && sensorActivado) {
            sensorActivado = false;
        }

        // CONTROL DE PARADA POR VUELTAS
        if (movimientoPorVueltas) {
            // Se detiene cuando alcanza las vueltas deseadas DESPUÉS del reset inicial
            if (vueltas_actuales >= cont_vueltas_objetivo) {
                detenerMotor();
                movimientoPorVueltas = false;
            }
        }

        // CONTROL DE PARADA POR HOME (CALIBRACIÓN)
        if (buscandoHomeFisico && lecturaHome) {
            detenerMotor();
            encoders[0].setCount(0); 
            buscandoHomeFisico = false;
            vueltas_actuales = 0;
        }

        telemetriaIndividual(0);
        lastVelTime = timeNow;
    }

    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        procesarComandosLabVIEW(cmd);
    }
}

void procesarComandosLabVIEW(String cmd) {
    // --- COMANDO HOME: BUSCA EL 0 Y SE QUEDA AHÍ ---
    if (cmd.startsWith("HOME:INIT:")) {
        iniciarBusquedaHome(cmd.substring(10).toInt());
        estadoActual = 1;
    }
    
    // --- COMANDO VUELTAS: BUSCA EL 0, RESETEA Y DA LA VUELTA ---
    else if (cmd.startsWith("MOV:1V:") || cmd.startsWith("MOV:10V:")) {
        int pwm = cmd.startsWith("MOV:1V:") ? cmd.substring(7).toInt() : cmd.substring(8).toInt();
        cont_vueltas_objetivo = cmd.startsWith("MOV:1V:") ? 1 : 10;
        estadoActual = cmd.startsWith("MOV:1V:") ? 2 : 3;

        // 1. Primero buscamos el inicio de la muesca (Home rápido)
        iniciarBusquedaHome(pwm);
        
        // 2. Esperamos activamente a que llegue al inicio de la muesca
        while(digitalRead(PIN_HOME) == HIGH) { delay(1); } 
        
        // 3. ¡AQUÍ ES EL 0 REAL! Reiniciamos todo justo al tocar la muesca
        encoders[0].setCount(0);
        vueltas_actuales = 0;
        movimientoPorVueltas = true;
        buscandoHomeFisico = false;
        
        // 4. Salimos de la muesca inicial para que el candado no cuente esta misma como "vuelta 1"
        sensorActivado = true; // El candado empieza cerrado porque estamos sobre la muesca
        moverMotor(0, pwm);
    }
}

void iniciarBusquedaHome(int pwm) {
    pwmActual[0] = (uint16_t)pwm;
    buscandoHomeFisico = true;
    movimientoPorVueltas = false;
    
    // Si ya estamos en el sensor, salimos para detectar el flanco de entrada de nuevo
    if (digitalRead(PIN_HOME) == LOW) {
        moverMotor(0, pwm);
        while(digitalRead(PIN_HOME) == LOW) { delay(1); }
        delay(100); 
    }
    moverMotor(0, pwm);
}

void detenerMotor() {
    ledcWrite(PIN_PWM_DER, 0);
    ledcWrite(PIN_PWM_IZQ, 0);
    pwmActual[0] = 0;
    estadoActual = 0;
}

void moverMotor(int id, int v) {
    if (v > 0) {
        ledcWrite(PIN_PWM_DER, abs(v));
        ledcWrite(PIN_PWM_IZQ, 0);
    } else if (v < 0) {
        ledcWrite(PIN_PWM_DER, 0);
        ledcWrite(PIN_PWM_IZQ, abs(v));
    } else {
        detenerMotor();
    }
}

void telemetriaIndividual(uint8_t id) {
    int32_t pasos = (int32_t)encoders[id].getCount();
    int16_t vueltas = (int16_t)vueltas_actuales;
    int16_t vel = velocidades[id];
    uint8_t motorData[10];
    memcpy(&motorData[0], &pasos, 4);
    memcpy(&motorData[4], &vueltas, 2);
    memcpy(&motorData[6], &vel, 2);
    motorData[8] = estadoActual;
    motorData[9] = (uint8_t)pwmActual[id]; 

    uint16_t sumaCrc = 0x01 + 11 + id;
    for(int i = 0; i < 10; i++) sumaCrc += motorData[i];

    Serial.write(0x3A);
    Serial.write(0x01);
    Serial.write(11);
    Serial.write(id);
    Serial.write(motorData, 10);
    Serial.write((uint8_t)(sumaCrc & 0xFF));
}