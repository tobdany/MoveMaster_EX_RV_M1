// ================= CONFIG =================
#define CENTRO     512
#define DEADZONE   100
#define FILTRO_N   3
#define RAMPA      30
#define TIMEOUT    300

// ================= VARIABLES =================
int joy[6];
int pwmActual[6] = {0,0,0,0,0,0};
unsigned long lastSignal = 0;

// Filtro promedio
int filtro[6][FILTRO_N];
int filtroIndex = 0;

// BTS7960 o LEDs
int RPWM[6] = {5, 7, 9, 11, 13, 45};
int LPWM[6] = {6, 8, 10, 12, 44, 46};
int EN[6]   = {22, 23, 24, 25, 26, 27};

// Bluetooth
String buffer = "";
bool recibiendo = false;

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);

  for (int i = 0; i < 6; i++) {
    pinMode(RPWM[i], OUTPUT);
    pinMode(LPWM[i], OUTPUT);
    pinMode(EN[i], OUTPUT);
    digitalWrite(EN[i], HIGH);
  }

  Serial.println("MEGA LISTO");
}

// ================= LOOP =================
void loop() {

  // ---- Recepción Bluetooth robusta ----
  while (Serial1.available()) {
    char c = Serial1.read();

    if (c == '<') {
      buffer = "";
      recibiendo = true;
    }
    else if (c == '>' && recibiendo) {
      recibiendo = false;

      if (procesarPaquete(buffer)) {
        lastSignal = millis();
        controlMotors();
      }
    }
    else if (recibiendo) {
      buffer += c;
    }
  }

  // ---- Failsafe ----
  if (millis() - lastSignal > TIMEOUT) {
    stopMotors();
  }
}

// ================= PROCESAR PAQUETE =================
bool procesarPaquete(String data) {
  int checksumRx = 0;
  int checksumCalc = 0;

  for (int i = 0; i < 7; i++) {
    int comma = data.indexOf(',');
    String token;

    if (comma == -1) token = data;
    else {
      token = data.substring(0, comma);
      data = data.substring(comma + 1);
    }

    if (i < 6) {
      joy[i] = token.toInt();
      checksumCalc += joy[i];
    } else {
      checksumRx = token.toInt();
    }
  }

  if (checksumCalc != checksumRx) {
    stopMotors();
    return false;
  }

  return true;
}

// ================= FILTRO =================
int aplicarFiltro(int canal, int valor) {
  filtro[canal][filtroIndex] = valor;
  int suma = 0;

  for (int i = 0; i < FILTRO_N; i++) {
    suma += filtro[canal][i];
  }

  return suma / FILTRO_N;
}

// ================= CONTROL MOTORES =================
void controlMotors() {

  for (int i = 0; i < 6; i++) {

    // 1️⃣ Filtro
    int filtrado = aplicarFiltro(i, joy[i]);

    // 2️⃣ Centro
    int desplazamiento = filtrado - CENTRO;

    // 3️⃣ Deadzone
    if (abs(desplazamiento) < DEADZONE) desplazamiento = 0;

    // 4️⃣ Mapeo a PWM
    int target = map(desplazamiento, -512, 512, -255, 255);

    // 5️⃣ Rampa rápida
    if (pwmActual[i] < target) pwmActual[i] += RAMPA;
    if (pwmActual[i] > target) pwmActual[i] -= RAMPA;

    int pwm = pwmActual[i];
    int outR = 0, outL = 0;

    if (pwm > 0) outR = pwm;
    else if (pwm < 0) outL = -pwm;

    analogWrite(RPWM[i], outR);
    analogWrite(LPWM[i], outL);
  }

  filtroIndex++;
  if (filtroIndex >= FILTRO_N) filtroIndex = 0;
}

// ================= STOP TOTAL =================
void stopMotors() {
  for (int i = 0; i < 6; i++) {
    analogWrite(RPWM[i], 0);
    analogWrite(LPWM[i], 0);
    pwmActual[i] = 0;
  }
}