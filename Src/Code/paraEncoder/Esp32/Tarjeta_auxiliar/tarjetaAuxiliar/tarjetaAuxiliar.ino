#include <Arduino.h>
#include <ESP32Encoder.h>

//-------------------------------
// --- HANDLER TAREAS ---
//-------------------------------
TaskHandle_t MuestreoHandler;
TaskHandle_t ControlHandler;
TaskHandle_t ReadSerialHandler; 

//-------------------------------
//--- ESTRUCTURAS ---
//-------------------------------
enum EstadoMotor {
    IDLE = 0,           
    MOVIENDO = 1,       
    META_ALCANZADA = 2, 
    HOMING = 3          
};

struct Motor_t {
    int pwmL;       
    int pwmR;                
    EstadoMotor estado=IDLE;     
};

struct __attribute__((packed)) MsgStatus_Sender_t {
    uint8_t header = 0x3A;
    uint8_t funcion = 0x07; 
    uint8_t payloadSize = 5;
    uint8_t estadoGripper;  
    uint8_t finalesCarrera; 
    uint8_t reserva1 = 0;
    uint8_t reserva2 = 0;
    uint8_t crc;
};

struct __attribute__((packed)) MsgDatos_Receiver_t {
    uint8_t header = 0x3A;
    uint8_t funcion;
    uint8_t payloadSize = 5;
    uint8_t pwm;
    uint8_t distancia;
    uint8_t direccion;
    uint8_t reserva1;
    uint8_t crcFinal;
};

//-------------------------------
// -- QUEUE & SEMAPHORE ---
//-------------------------------
QueueHandle_t colaControl;
SemaphoreHandle_t xSerial2Mutex;

//-------------------------------
// --- GLOBALES ---
//-------------------------------
Motor_t structGripper;

//Configuración de pines
int pinesFinalCarrera[] = {27, 4, 14, 15, 16};  //16 es rx2
int numeroFinalesCarrera = sizeof(pinesFinalCarrera) / sizeof(pinesFinalCarrera[0]);
int frenosHombro = 17;
int frenosCod = 21;

uint8_t heartbeatCount = 0;

// Variables de control de tiempo para el Gripper
uint32_t tiempoInicioGripper = 0;
uint32_t tiempoObjetivo = 0; // Cuántos ms debe moverse

//-------------------------------
//--- FUNCIONES DE APOYO ---
//-------------------------------

uint8_t obtenerEstadoSwitches() {
    uint8_t registro = 0;
    //Serial.print("LS: ");
    for(int i = 0; i < numeroFinalesCarrera; i++) {
        // (Normalmente Cerrados), si se abre da 0.
        // Si quieres que el bit sea 1 al presionarse: !(digitalRead)
        registro |= (digitalRead(pinesFinalCarrera[i]) << i);
        /*Serial.print(digitalRead(pinesFinalCarrera[i]));
        Serial.print(" , ");*/
    }
    //Serial.println();
    return registro;
}

void enviarEstadoGeneral() {
    if (xSemaphoreTake(xSerial2Mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        MsgStatus_Sender_t msg;
        msg.estadoGripper = (uint8_t)structGripper.estado;
        msg.finalesCarrera = obtenerEstadoSwitches();
        
        uint8_t* ptr = (uint8_t*)&msg;
        uint8_t suma = 0;
        for(int i = 0; i < sizeof(MsgStatus_Sender_t) - 1; i++) suma += ptr[i];
        msg.crc = suma;

        Serial2.write((uint8_t*)&msg, sizeof(MsgStatus_Sender_t));
        xSemaphoreGive(xSerial2Mutex);
    }
}

void funcionEmergencia(uint8_t finalCarreraData) {
    analogWrite(structGripper.pwmL, 0);
    analogWrite(structGripper.pwmR, 0);
    structGripper.estado = IDLE;
    xQueueReset(colaControl);

    // Enviamos el estado inmediatamente para avisar del error
    enviarEstadoGeneral();
    
    //Serial.print("EMERGENCIA Switches: ");
    //Serial.println(finalCarreraData, BIN);
}



void ejecutarHomingManual() {
    Serial.println("Iniciando Homing Manual...");
    structGripper.estado = HOMING;

    analogWrite(structGripper.pwmL, 0);
    analogWrite(structGripper.pwmR, 80); 

    vTaskDelay(pdMS_TO_TICKS(4000));

    analogWrite(structGripper.pwmR, 0);
    structGripper.estado = IDLE;
    
    Serial.println("Homing completado.");
    enviarEstadoGeneral();
}

//-------------------------------
//--- TAREAS ---
//-------------------------------

void tareaMuestreo(void * parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t ultimoEnvio = millis();

    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
        uint8_t switches = obtenerEstadoSwitches();
        
        if (switches != 0x1F) { 
            funcionEmergencia(switches);
        } 
        
        // Reporte periódico (Heartbeat unificado)
        if (millis() - ultimoEnvio > 500) {
            enviarEstadoGeneral();
            ultimoEnvio = millis();
        }
    }
}

void tareaLeerSerial(void *pvParameters) {
    const size_t TAMANO_PACKET = sizeof(MsgDatos_Receiver_t);
    MsgDatos_Receiver_t rawMsg;
    for (;;) {
        if (Serial2.available() >= TAMANO_PACKET) {
            if (Serial2.peek() != 0x3A) {
                Serial2.read(); continue;
            }
            Serial2.readBytes((uint8_t*)&rawMsg, TAMANO_PACKET);

            uint8_t crcCalculado = 0;
            uint8_t* ptr = (uint8_t*)&rawMsg;
            for(int i = 0; i < TAMANO_PACKET - 1; i++) crcCalculado += ptr[i];

            if (crcCalculado == rawMsg.crcFinal) {
                xQueueSend(colaControl, &rawMsg, pdMS_TO_TICKS(10));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void tareaControl(void * parameter) {
    MsgDatos_Receiver_t cmd;
    uint32_t tiempoInicio = 0;
    uint32_t duracionMeta = 0;

    for(;;) {
        if (xQueueReceive(colaControl, &cmd, pdMS_TO_TICKS(0))) {
            if (cmd.funcion == MOVIENDO) {
                structGripper.estado = MOVIENDO;
                tiempoInicio = millis();
                duracionMeta = cmd.distancia * 10;

                if (cmd.direccion == 1) { // Usando el campo 'direccion' de tu estructura
                    analogWrite(structGripper.pwmL, 0);
                    analogWrite(structGripper.pwmR, cmd.pwm);
                } else { 
                    analogWrite(structGripper.pwmL, cmd.pwm);
                    analogWrite(structGripper.pwmR, 0);
                }

                enviarEstadoGeneral(); // Avisar que empezó a moverse
            } 
            else if (cmd.funcion == HOMING) {
                ejecutarHomingManual();
                enviarEstadoGeneral();
            }
            else if (cmd.funcion == IDLE) {
                structGripper.estado = IDLE;
                analogWrite(structGripper.pwmL, 0);
                analogWrite(structGripper.pwmR, 0);
                enviarEstadoGeneral();
            }
        }

        if (structGripper.estado == MOVIENDO && (millis() - tiempoInicio >= duracionMeta)) {
            analogWrite(structGripper.pwmL, 0);
            analogWrite(structGripper.pwmR, 0);
            structGripper.estado = META_ALCANZADA;
            enviarEstadoGeneral(); // Avisar que llegó
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}



//-------------------------------
//--- SETUP & LOOP ---
//-------------------------------

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 13, 12);
    
    xSerial2Mutex = xSemaphoreCreateMutex();

    // Configuración Gripper
    structGripper.pwmL = 25;
    structGripper.pwmR = 26;

    ledcAttach(structGripper.pwmL, 5000, 8);
    ledcAttach(structGripper.pwmR, 5000, 8);

    // Asegurar motores apagados al arranque
    analogWrite(structGripper.pwmL, 0);
    analogWrite(structGripper.pwmR, 0);

    for(int i = 0; i < numeroFinalesCarrera; i++) {
        pinMode(pinesFinalCarrera[i], INPUT_PULLUP); // Recomendado usar PULLUP
    }

    //Configuración de los frenos
    pinMode(frenosHombro, OUTPUT);
    pinMode(frenosCod, OUTPUT);

    // Salida inicial de frenos
    digitalWrite(frenosHombro, LOW);
    digitalWrite(frenosCod, LOW);

    // Cola configurada con el tamaño correcto (Receiver)
    colaControl = xQueueCreate(10, sizeof(MsgDatos_Receiver_t));

    xTaskCreatePinnedToCore(tareaMuestreo, "Muestreo", 4096, NULL, 3, &MuestreoHandler, 0);
    xTaskCreatePinnedToCore(tareaLeerSerial, "ReadSerial", 4096, NULL, 2, &ReadSerialHandler, 0);
    xTaskCreatePinnedToCore(tareaControl, "Control", 4096, NULL, 1, &ControlHandler, 1);
}


void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}