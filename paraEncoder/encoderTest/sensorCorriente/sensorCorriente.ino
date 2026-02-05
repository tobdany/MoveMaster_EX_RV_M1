// Configuración de pines
const int pinL_IS = A1; // Sensor para giro a la izquierda
const int pinR_IS = A0; // Sensor para giro a la derecha
const int L_PWM = 6;
const int R_PWM = 5;

// Parámetros de cálculo
const float R_IS = 2200.0;   // Tu resistencia de 2.2k ohms
const float K_ILIS = 8500.0; // Factor de relación del BTS7960
const float V_REF = 1.1;     // Voltaje de referencia del Arduino (5V)
const int numLecturas = 100;  // Cantidad de lecturas para el promedio

void setup() {
  Serial.begin(115200);
  analogReference(INTERNAL);

  pinMode(L_PWM, OUTPUT);
  pinMode(R_PWM, OUTPUT);
  
  // Prueba de motor: Girando en un sentido a media velocidad
  analogWrite(R_PWM, 0); 
  analogWrite(L_PWM, 255);
  
  Serial.println("--- Monitor de Corriente BTS7960 (Promediado) ---");
}

void loop() {
  long sumaL = 0;
  long sumaR = 0;

  // 1. Tomar 10 lecturas rápidas
  for (int i = 0; i < numLecturas; i++) {
    sumaL += analogRead(pinL_IS);
    sumaR += analogRead(pinR_IS);
    delay(1); // Pequeña pausa entre micro-lecturas para estabilidad
  }

  // 2. Calcular el promedio de los valores crudos
  float promedioL = (float)sumaL / numLecturas;
  float promedioR = (float)sumaR / numLecturas;

  // 3. Convertir promedio a Voltaje
  float volL = (promedioL * V_REF) / 1023.0;
  float volR = (promedioR * V_REF) / 1023.0;
  Serial.print("Voltaje L: ");
  Serial.print(volL,4);
  Serial.print("Voltaje R: ");
  Serial.println(volR,4);

  // 4. Calcular Corriente usando la fórmula: I = (V * K_ILIS) / R_IS
  float currentL = (volL * K_ILIS) / R_IS;
  float currentR = (volR * K_ILIS) / R_IS;

  // 5. Corriente total
  float currentTotal = currentL + currentR;

  // Mostrar resultados
  Serial.print("L: ");
  Serial.print(currentL, 4); 
  Serial.print(" A | R: ");
  Serial.print(currentR, 4);
  Serial.print(" A | TOTAL: ");
  Serial.print(currentTotal, 4);
  Serial.println(" A");
  // Alerta de seguridad por saturación del pin (cerca de 5V)
  // Con 2.2k, esto ocurre cerca de los 19 Amperios.
  if (volL >= 4.5 || volR >= 4.5) {
    Serial.println(" [!] ALERTA: Corriente muy alta para la resistencia de 2.2k");
  }

  delay(200); // Pausa para que el Monitor Serie sea legible
}