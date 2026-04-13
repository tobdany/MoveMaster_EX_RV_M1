#include <Arduino.h>

//-------------------------------
// --- HANDLER TAREAS ---
//-------------------------------
TaskHandle_t MuestreoHandler;
TaskHandle_t ReadSerialHandler; 
TaskHandle_t WriteSerialHandler;

//-------------------------------
//--- ESTRUCTURAS ---
//-------------------------------
enum EstadoMotor {
    IDLE,
    MOVIENDO,
    META_ALCANZADA,
    HOMING
};

struct Motor_t {
    int32_t pasosActuales;  
    int32_t metaPasos;
    int pwmActual;
    EstadoMotor estado;
};

struct __attribute__((packed)) MsgDatos_t {
     uint8_t header = 0x3A;
     uint8_t funcion;
     uint8_t payloadSize = 25;
     uint8_t globalBuffer[24];
     uint8_t crcFinal;
};

//-------------------------------
// -- CONSTANTES ---
//-------------------------------
const int NUM_MOTORES = 6;
const int NUM_ENCODERS = 5; 

//-------------------------------
// -- QUEUE ---
//-------------------------------
QueueHandle_t colaTelemetria;

//-------------------------------
// --- GLOBALES ---
//-------------------------------
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; 
Motor_t ArrayMotores[NUM_MOTORES];

//-------------------------------
// --- TAREA: SIMULACIÓN DE MUESTREO ---
//-------------------------------
//-------------------------------
// --- TAREA: SIMULACIÓN DE MUESTREO ---
//-------------------------------
void tareaMuestreo(void * parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriodo = pdMS_TO_TICKS(4); 
    uint8_t localBuffer[24]; 

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xPeriodo);
        memset(localBuffer, 0, 24); 
        
        for(int i = 0; i < NUM_ENCODERS; i++){
            // Lógica de rangos: 
            // Motor 0: 0-999, Motor 1: 1000-1999, etc.
            int baseRango = i * 1000;
            
            // Incrementamos y aplicamos un residuo para que no salga de su rango (0-999)
            ArrayMotores[i].pasosActuales++;
            if (ArrayMotores[i].pasosActuales >= 1000) {
                ArrayMotores[i].pasosActuales = 0;
            }

            // El valor final que mandamos es la base + el contador actual
            int32_t pasosAMandar = baseRango + ArrayMotores[i].pasosActuales;
            
            // Copiamos al buffer global (Little Endian)
            memcpy(&localBuffer[i * 4], &pasosAMandar, 4);
        }

        MsgDatos_t clusterLocalMsg;
        clusterLocalMsg.funcion = 0x01; 
        memcpy(clusterLocalMsg.globalBuffer, localBuffer, 24);

        // Cálculo de CRC (Se mantiene igual, sumando todos los bytes)
        uint16_t sumaCrc = 0;
        sumaCrc += clusterLocalMsg.header;
        sumaCrc += clusterLocalMsg.funcion;
        sumaCrc += clusterLocalMsg.payloadSize;
        for(int i = 0; i < 24; i++) sumaCrc += clusterLocalMsg.globalBuffer[i];
        clusterLocalMsg.crcFinal = (uint8_t)(sumaCrc & 0xFF);

        if (xQueueSend(colaTelemetria, &clusterLocalMsg, 0) != pdPASS) {
            // Cola llena
        }
    }
}

//-------------------------------
// --- TAREA: LEER COMANDOS (SIMULADO) ---
//-------------------------------
void ReadSerial(void * parameter) {
    for (;;) {
        if (Serial.available() > 0) {
            String cmd = Serial.readStringUntil('\n');
            int pwmLog = 0;

            if (cmd.startsWith("MOV:")) {
                pwmLog = cmd.substring(4).toInt();
            }

            // Actualización segura de parámetros
            portENTER_CRITICAL(&mux);
            for(int i=0; i<NUM_MOTORES; i++){
                ArrayMotores[i].pwmActual = pwmLog;
                if(pwmLog != 0) ArrayMotores[i].estado = MOVIENDO;
                else ArrayMotores[i].estado = IDLE;
            }
            portEXIT_CRITICAL(&mux);

            Serial.print("[LOG] PWM Objetivo actualizado a: ");
            Serial.println(pwmLog);
        }
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}

//-------------------------------
// --- TAREA: ESCRITURA SERIAL (BINARIO) ---
//-------------------------------
void WriteSerial(void * parameter){
    MsgDatos_t msgParaEnviar; 
    
    for(;;){
        if (xQueueReceive(colaTelemetria, &msgParaEnviar, portMAX_DELAY) == pdPASS) {
            // Mandar a LabVIEW en binario
            Serial.write(msgParaEnviar.header); 
            Serial.write(msgParaEnviar.funcion); 
            Serial.write(msgParaEnviar.payloadSize);      
            Serial.write(msgParaEnviar.globalBuffer, 24); 
            Serial.write(msgParaEnviar.crcFinal);
        }
    }
}

//-------------------------------
// SETUP
//-------------------------------
void setup() {
    Serial.begin(921600);
    delay(1000);
    Serial.println("\n*******************************");
    Serial.println("MODO SIMULACION (SIN PINES)");
    Serial.println("*******************************");

    // Inicializar estructuras
    for(int i = 0; i < NUM_MOTORES; i++) {
        ArrayMotores[i].pasosActuales = 0;
        ArrayMotores[i].pwmActual = 0;
        ArrayMotores[i].estado = IDLE;
    }

    colaTelemetria = xQueueCreate(100, sizeof(MsgDatos_t));

    // Crear tareas
    xTaskCreatePinnedToCore(tareaMuestreo, "TaskMuestreo", 4096, NULL, 3, &MuestreoHandler, 1);
    xTaskCreatePinnedToCore(ReadSerial, "TaskReadSerial", 4096, NULL, 1, &ReadSerialHandler, 0);
    xTaskCreatePinnedToCore(WriteSerial, "TaskWriteSerial", 4096, NULL, 1, &WriteSerialHandler, 0);

    Serial.println("[SETUP] Sistema listo para LabVIEW.");
}

void loop() {
    // El loop de FreeRTOS no se usa
    vTaskDelay(pdMS_TO_TICKS(1000));
}