#include <Servo.h>

Servo miServo;

const int PIN_SERVO = A3;

void setup() {
  Serial.begin(115200);

  // Servo en pin A3 (S / amarillo)
  miServo.attach(PIN_SERVO);
  
  // Posicion inicial
  Serial.println("Posicion al frente");
  miServo.write(90);
  delay(1000);
}

void loop() {
  miServo.write(90);
  delay(1500);

  miServo.write(30);
  delay(1500);

  miServo.write(90);
  delay(1500);

  miServo.write(150);
  delay(1500);
}
