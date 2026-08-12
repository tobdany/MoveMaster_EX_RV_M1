// ================================================================
// RECEPTOR MEGA: CONTROL DE 6 MOTORES BTS7960 + MONITOR SERIAL DEBUG
// ================================================================

#define CENTRO      512
#define DEADZONE    80   // Ignora pequeños movimientos del joystick
#define FILTRO_N    3    // Suavizado de señal
#define RAMPA        40   // Velocidad de respuesta (0-255)
#define TIMEOUT     500  // Failsafe: se detiene si no hay señal en 0.5 seg

// Variables de control
int joy[6];
int pwmActual[6] = {0,0,0,0,0,0}; 
unsigned long lastSignal = 0;

// Pines para los 6 drivers BTS7960
int RPWM[6] = {4, 2, 6, 8, 10, 12};
int LPWM[6] = {5, 3, 7, 9, 11, 13};

String entradaBT = ""; 
bool capturando = false;

void setup() {
  // Serial 0: Monitor PC (Configurar a 115200)
  Serial.begin(115200); 
  
  // Serial 1: Bluetooth (Pines 19 RX y 18 TX)
  Serial1.begin(9600); 
  
  for (int i = 0; i < 6; i++) {
    pinMode(RPWM[i], OUTPUT);
    pinMode(LPWM[i], OUTPUT);
  }

  Serial.println("\n--- RECEPTOR MEGA INICIADO (MODO DEBUG) ---");
  Serial.println("Esperando datos con formato <J1,J2,J3,J4,J5,J6,Sum>...");
}

void loop() {
  // 1. LECTURA DEL PUERTO SERIAL 1 (BLUETOOTH)
  while (Serial1.available()) {
    char c = Serial1.read();

    if (c == '<') {
      entradaBT = "";
      capturando = true;
    } 
    else if (c == '>' && capturando) {
      capturando = false;
      
      // MOSTRAR TRAMA RECIBIDA
      Serial.print("\n>>> Trama Cruda: <");
      Serial.print(entradaBT);
      Serial.println(">");
      
      if (procesarPaquete(entradaBT)) {
        lastSignal = millis(); 
        Serial.println("Checksum OK. Actualizando motores...");
        actualizarMotores();   
      } else {
        Serial.println("Error: Checksum NO coincide.");
      }
    } 
    else if (capturando) {
      entradaBT += c;
    }
  }

  // 2. FAILSAFE: Si pasa mucho tiempo sin señal, apagar todo
  if (millis() - lastSignal > TIMEOUT) {
    if(pwmActual[0] != 0 || pwmActual[1] != 0) { // Solo imprime si estaban encendidos
       Serial.println("!!! FAILSAFE ACTIVADO: Señal perdida !!!");
    }
    detenerMotores();
  }
}

// Función para separar los datos por comas y validar Checksum
bool procesarPaquete(String data) {
  int ckSumCalc = 0;
  int ckSumRecibido = 0;
  String tempData = data; // Copia para no destruir la original
  
  for (int i = 0; i < 7; i++) {
    int comma = tempData.indexOf(',');
    String valorStr = (comma == -1) ? tempData : tempData.substring(0, comma);
    tempData = tempData.substring(comma + 1);

    int valorInt = valorStr.toInt();

    if (i < 6) {
      joy[i] = valorInt;
      ckSumCalc += valorInt;
      // Opcional: ver valores individuales
      Serial.print("J"); Serial.print(i+1); Serial.print(":"); Serial.print(valorInt); Serial.print(" ");
    } else {
      ckSumRecibido = valorInt;
      Serial.print("| Sum Recibido: "); Serial.println(ckSumRecibido);
    }
  }
  
  return (ckSumCalc == ckSumRecibido);
}

void actualizarMotores() {
  Serial.print("PWM Salida: ");
  for (int i = 0; i < 6; i++) {
    int desplazamiento = joy[i] - CENTRO;

    if (abs(desplazamiento) < DEADZONE) desplazamiento = 0;

    int target = map(desplazamiento, -512, 512, -255, 255);

    // Rampa
    if (pwmActual[i] < target) pwmActual[i] = min(pwmActual[i] + RAMPA, target);
    else if (pwmActual[i] > target) pwmActual[i] = max(pwmActual[i] - RAMPA, target);

    // Salida física
    if (pwmActual[i] > 0) {
      analogWrite(RPWM[i], pwmActual[i]);
      analogWrite(LPWM[i], 0);
    } else if (pwmActual[i] < 0) {
      analogWrite(RPWM[i], 0);
      analogWrite(LPWM[i], abs(pwmActual[i]));
    } else {
      analogWrite(RPWM[i], 0);
      analogWrite(LPWM[i], 0);
    }

    Serial.print(pwmActual[i]);
    Serial.print("\t");
  }
  Serial.println(); 
}

void detenerMotores() {
  for (int i = 0; i < 6; i++) {
    analogWrite(RPWM[i], 0);
    analogWrite(LPWM[i], 0);
    pwmActual[i] = 0;
  }
}