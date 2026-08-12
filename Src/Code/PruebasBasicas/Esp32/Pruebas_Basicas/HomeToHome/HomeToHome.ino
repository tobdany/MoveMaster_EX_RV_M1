#include <Arduino.h>
#include <ESP32Encoder.h>

// --- CONFIGURACIÓN DE PINES ---
const int PIN_A = 19;
const int PIN_B = 18;
const int PIN_Z = 34;
const int PIN_PWM_DER = 25; 
const int PIN_PWM_IZQ = 26;

const int PASOS_OBJETIVO = 400; // 200 PPR * 2 (HalfQuad)


int PWM_VELOCIDAD = 40;   
bool flagHome = false;

volatile bool homeDetectado = false;
volatile long pasosAcumulados = 0;


enum EstadoRobot {
  IDLE,              // Vale 0
  BUSCANDO_HOME,     // Vale 1
  MOVIENDO_VUELTAS,  // Vale 2
  ERROR_SISTEMA,     // Vale 3
  REPORTAR
};

EstadoRobot estadoActual = IDLE;

ESP32Encoder encoder;



void moverMotor(int velocidad){
    ledcWrite(PIN_PWM_DER, velocidad); 
    ledcWrite(PIN_PWM_IZQ, 0);
}

void frenarMotor() {
    // Freno rápido: apagamos ambos
    ledcWrite(PIN_PWM_DER, 0);
    ledcWrite(PIN_PWM_IZQ, 0);
}

void reportarDatos(){
    long pasosActuales = (long)encoder.getCount();
    Serial.print("Progreso: ");
    //Serial.println(pasosActuales);
    Serial.print(pasosAcumulados);
    Serial.print(", PWM: ");
    Serial.println(PWM_VELOCIDAD);

}

void IRAM_ATTR isr_home() {
   pasosAcumulados = encoder.getCount();
    encoder.clearCount();
    homeDetectado = true; 
    //pasosAcumulados=(long)encoder.getCount();
}





void setup() {
  Serial.begin(115200);

  ledcAttach(PIN_PWM_DER, 5000, 8);
  ledcAttach(PIN_PWM_IZQ, 5000, 8);

   ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(PIN_A, PIN_B);
  encoder.setFilter(10);
  encoder.clearCount();
    
    // Configuración del Pin Z (HOME)
  //pinMode(PIN_Z, INPUT_PULLUP);
  //pinMode(PIN_Z, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_Z), isr_home, FALLING);



}



void loop() {
  switch (estadoActual) {
    case IDLE:
      homeDetectado=false;
      estadoActual=BUSCANDO_HOME;
      PWM_VELOCIDAD = (PWM_VELOCIDAD + 25) % 256;
      if (PWM_VELOCIDAD < 40) PWM_VELOCIDAD = 40;
      moverMotor(PWM_VELOCIDAD);
      //Serial.println("Buscando Home");
      break;

    case BUSCANDO_HOME:

      if (homeDetectado) {
        //encoder.clearCount();
        //frenarMotor();
        //delay(1);
        homeDetectado=false;
        estadoActual = MOVIENDO_VUELTAS; // Cambiamos de estado
        //Serial.println("Dando una vuelta");
        moverMotor(PWM_VELOCIDAD);
      }
      break;

    case MOVIENDO_VUELTAS:
      if(homeDetectado) {
          estadoActual=REPORTAR;
        }else{
            //homeDetectado = false;
        }
      break;

    case REPORTAR:
      frenarMotor();
      reportarDatos();
      delay(500);
      estadoActual=IDLE;
    break;
  }

}

