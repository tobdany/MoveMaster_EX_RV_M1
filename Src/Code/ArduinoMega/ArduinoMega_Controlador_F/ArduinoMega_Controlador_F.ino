#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

// --- CONFIGURACIÓN DE PINES ---
const uint8_t NUM_MOTORES = 6;
const uint8_t NUM_ENCODERS = 5;
const uint8_t PIN_RPWM[NUM_MOTORES] = {2, 4, 6, 8, 10, 12}; 
const uint8_t PIN_LPWM[NUM_MOTORES] = {3, 5, 7, 9, 11, 13};
const uint8_t PIN_ENCODER_A[NUM_ENCODERS] = {39, 35, 31, 27, 23};
const uint8_t PIN_ENCODER_B[NUM_ENCODERS] = {41, 37, 33, 29, 25};
const uint8_t PIN_FRENO[2] = {14, 15};

// --- ESTRUCTURAS ---
struct __attribute__((packed)) MsgDatos_t {
    uint8_t header = 0x3A; 
    uint8_t funcion = 0x01;
    uint8_t edoMotores[3] = {0,0,0};
    uint8_t finalesCarrera = 0;
    int32_t pasos[6] = {0,0,0,0,0,0};
    uint8_t crcFinal;
};

struct EncoderHw_t {
    volatile uint8_t *portA, *portB;
    uint8_t maskA, maskB;
};

// --- VARIABLES GLOBALES ---
volatile int32_t encoderCount[NUM_ENCODERS] = {0,0,0,0,0};
volatile uint8_t encoderLastState[NUM_ENCODERS];
EncoderHw_t encoderHw[NUM_ENCODERS];
// Tabla de verdad para resolución X2 (400 pulsos)
// Solo reacciona a cambios en una fase para reducir la resolución a la mitad
const int8_t encoderTable[16] = {
    0, 0, 0, 0,  // Sin cambio
    1, 0, 0, -1, // Cambios detectados
   -1, 0, 0, 1,  // Cambios detectados
    0, 0, 0, 0   // Sin cambio
};

uint32_t lastControlUs = 0;
const uint32_t CONTROL_PERIOD_US = 4000UL; // 20ms para esta prueba (más lento es mejor para ver datos)

// --- LÓGICA DE ENCODERS (TIMER 5) ---
ISR(TIMER5_COMPA_vect) {
    for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
        uint8_t a = (*(encoderHw[i].portA) & encoderHw[i].maskA) ? 1 : 0;
        uint8_t b = (*(encoderHw[i].portB) & encoderHw[i].maskB) ? 1 : 0;
        uint8_t estadoActual = (a << 1) | b;
        encoderCount[i] += encoderTable[(encoderLastState[i] << 2) | estadoActual];
        encoderLastState[i] = estadoActual;
    }
}

void configurarTimer5() {
    noInterrupts();
    TCCR5A = 0; TCCR5B = (1 << WGM52) | (1 << CS51); // CTC, prescaler 8
    OCR5A = 49; // 40kHz aprox
    TIMSK5 = (1 << OCIE5A);
    interrupts();
}

// --- CONTROL DE MOTORES ---
void moverMotorPrueba(uint8_t i, int16_t v) {
    // Liberar frenos para la prueba
    digitalWrite(PIN_FRENO[0], HIGH); 
    digitalWrite(PIN_FRENO[1], HIGH);
    
    if (v > 0) { analogWrite(PIN_RPWM[i], v); analogWrite(PIN_LPWM[i], 0); }
    else if (v < 0) { analogWrite(PIN_RPWM[i], 0); analogWrite(PIN_LPWM[i], -v); }
    else { analogWrite(PIN_RPWM[i], 0); analogWrite(PIN_LPWM[i], 0); }
}

void setup() {
    Serial.begin(500000); // LabVIEW
    
    // Configurar Motores y Frenos
    for (int i = 0; i < NUM_MOTORES; i++) {
        pinMode(PIN_RPWM[i], OUTPUT); 
        pinMode(PIN_LPWM[i], OUTPUT);
    }
    pinMode(PIN_FRENO[0], OUTPUT); 
    pinMode(PIN_FRENO[1], OUTPUT);

    // Configurar Encoders Hardware
    for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
        pinMode(PIN_ENCODER_A[i], INPUT_PULLUP);
        pinMode(PIN_ENCODER_B[i], INPUT_PULLUP);
        encoderHw[i].portA = portInputRegister(digitalPinToPort(PIN_ENCODER_A[i]));
        encoderHw[i].maskA = digitalPinToBitMask(PIN_ENCODER_A[i]);
        encoderHw[i].portB = portInputRegister(digitalPinToPort(PIN_ENCODER_B[i]));
        encoderHw[i].maskB = digitalPinToBitMask(PIN_ENCODER_B[i]);
    }

    configurarTimer5();
    lastControlUs = micros();
}

void loop() {
    if (micros() - lastControlUs >= CONTROL_PERIOD_US) {
        lastControlUs += CONTROL_PERIOD_US;

        // --- PRUEBA SECUENCIAL ---
        // Esto moverá el motor 0 suavemente de ida y vuelta para probar encoder
        static int16_t testPWM = 40;
        static uint16_t counter = 0;
        counter++;
        
        if(counter < 100) moverMotorPrueba(0, testPWM);      // 2 seg adelante
        else if(counter < 200) moverMotorPrueba(0, -testPWM); // 2 seg atrás
        else counter = 0;

        // --- TELEMETRÍA A LABVIEW ---
        MsgDatos_t msg;
        for(int i=0; i<NUM_ENCODERS; i++) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
               msg.pasos[i] = encoderCount[i];
               //*****************************************************************************************************************************SIMULACION
               //msg.pasos[i] = (int32_t)(i * 1000) + random(1000);
            }
        }

        uint16_t sumaCrc = 0;
        sumaCrc += msg.header;
        sumaCrc += msg.funcion;
        sumaCrc += msg.finalesCarrera;
        for(int i=0; i<3; i++) sumaCrc += msg.edoMotores[i];

        // Accedemos a los 24 bytes de 'pasos' usando un puntero uint8_t
        uint8_t* pasosBytes = (uint8_t*)msg.pasos; 
        for(int i = 0; i < 24; i++) {
            sumaCrc += pasosBytes[i];
        }
        msg.crcFinal = (uint8_t)(sumaCrc & 0xFF);
        
        Serial.write((uint8_t*)&msg, sizeof(msg));
    }
}