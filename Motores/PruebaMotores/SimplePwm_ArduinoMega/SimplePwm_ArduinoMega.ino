// ================= CONFIG =================
// Definimos la velocidad de prueba (0 a 255)
const int VELOCIDAD_PRUEBA = 50; 

// Pines para BTS7960 o LEDs (Se mantienen igual a tu esquema)
int RPWM[6] = {5, 7, 9, 11, 13, 45};
int LPWM[6] = {6, 8, 10, 12, 44, 46};
int EN[6]   = {22, 23, 24, 25, 26, 27};

// ================= SETUP =================
void setup() {
  Serial.begin(9600);

  for (int i = 0; i < 6; i++) {
    pinMode(RPWM[i], OUTPUT);
    pinMode(LPWM[i], OUTPUT);
    pinMode(EN[i], OUTPUT);
    
    // Activamos los drivers
    digitalWrite(EN[i], HIGH);
  }

  Serial.println("--- INICIANDO PRUEBA DE MOTORES ---");
  Serial.print("Velocidad establecida en: ");
  Serial.println(VELOCIDAD_PRUEBA);
}

// ================= LOOP =================
void loop() {
  // Aplicamos la velocidad a los 6 canales
  for (int i = 0; i < 6; i++) {
    // Para girar en un sentido: RPWM con valor, LPWM en 0
    analogWrite(RPWM[i], VELOCIDAD_PRUEBA);
    analogWrite(LPWM[i], 0);
  }

  // Monitorización simple
  Serial.println("Motores girando...");
  delay(1000); 
}