// Pines para el BTS7960
const int RPWM = 25; // Giro derecha
const int LPWM = 26; // Giro izquierda
const int EN   = 27; // R_EN y L_EN unidos a este pin

void setup() {
  Serial.begin(115200);

  // Configurar pines como salida
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  pinMode(EN, OUTPUT);

  // Habilitar el driver (R_EN y L_EN deben recibir voltaje)
  digitalWrite(EN, HIGH); 
  
  Serial.println("Control de Motor con analogWrite Listo");
}

void loop() {
  // --- Giro Derecha ---
  Serial.println("Derecha...");
  analogWrite(RPWM, 180); // Velocidad (0 a 255)
  analogWrite(LPWM, 0);   // Aseguramos que el otro lado esté apagado
  delay(3000);

  // --- Parada ---
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 0);
  delay(1000);

  // --- Giro Izquierda ---
  Serial.println("Izquierda...");
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 180);
  delay(3000);

  // --- Parada ---
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 0);
  delay(1000);
}