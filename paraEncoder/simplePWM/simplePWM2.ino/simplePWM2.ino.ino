// Control de motor con BTS7960
// Oscar Gonzalez - BricoGeek.com

//const int L_EN = 8;
//const int R_EN = 8;
const int L_PWM = 6;
const int R_PWM = 5;

void setup() {

 pinMode(L_PWM, OUTPUT);
 pinMode(R_PWM, OUTPUT); 


  int velocidad = 255; // Valor entre 0 y 254
  analogWrite(R_PWM, velocidad);
  analogWrite(L_PWM, 0);
}

void loop() {

}
