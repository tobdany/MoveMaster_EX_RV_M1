#include <SoftwareSerial.h>

SoftwareSerial bt(2, 3); // RX, TX

void setup() {
  Serial.begin(9600);
  bt.begin(38400);  // Velocidad en modo AT
}

void loop() {
  if (bt.available()) {
    Serial.write(bt.read());
  }
  if (Serial.available()) {
    bt.write(Serial.read());
  }
}