float Sensibilidad=0.100; //sensibilidad en Voltios/Amperio para sensor de 5A

void setup() {
  
  Serial.begin(115200);
}

void loop() {
  
  float I=get_corriente(200);//obtenemos la corriente promedio de 500 muestras 

  delay(100);     
}

float get_corriente(int n_muestras)
{
  float voltajeSensor=0;
  double voltajeSensorTotal=0;
  float corriente=0;
  for(int i=0;i<n_muestras;i++)
  {
    voltajeSensor = analogRead(A0) * (5.0 / 1023.0);////lectura del sensor
    voltajeSensorTotal+=voltajeSensor;
  }
  voltajeSensor=voltajeSensorTotal/n_muestras;

  corriente=(voltajeSensor-2.5)/Sensibilidad; //Ecuación  para obtener la corriente
  Serial.print("V: ");
  Serial.print(voltajeSensor,4); 
  Serial.print(", C: ");
  Serial.println(corriente,4); 

  return(corriente);
}