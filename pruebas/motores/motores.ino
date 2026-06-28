#include <Arduino.h>

const int PIN_DIR_B = 2;
const int PIN_PWM_B = 5;

const int PIN_DIR_A = 4;
const int PIN_PWM_A = 6;


void setup() {
  pinMode(PIN_DIR_B, OUTPUT);
  pinMode(PIN_PWM_B, OUTPUT);
  pinMode(PIN_DIR_A, OUTPUT);
  pinMode(PIN_PWM_A, OUTPUT);
}

void loop() {
  // === 1. AVANZA ===
  digitalWrite(PIN_DIR_B, HIGH);
  analogWrite(PIN_PWM_B, 100);
  digitalWrite(PIN_DIR_A, HIGH);
  analogWrite(PIN_PWM_A, 100);
  delay(1500);  // Camina en línea recta durante 1.5 segundos

  // === 2. FRENA CORTO (Para estabilizar el chasis antes del giro) ===
  digitalWrite(PIN_DIR_B, LOW);
  analogWrite(PIN_PWM_B, 0);
  digitalWrite(PIN_DIR_A, LOW);
  analogWrite(PIN_PWM_A, 0);
  delay(300);  // Pausa de 0.3 segundos

  // === 3. GIRA 90° A LA DERECHA ===
  // Usamos tu lógica confirmada de giro a la derecha
  digitalWrite(PIN_DIR_B, HIGH);
  analogWrite(PIN_PWM_B, 100);
  digitalWrite(PIN_DIR_A, LOW);
  analogWrite(PIN_PWM_A, 100);

  // REGULA ESTE TIEMPO para clavar los 90 grados exactos en tu suelo:
  delay(600);

  // === 4. FRENA ANTES DE REPETIR EL BUCLE ===
  digitalWrite(PIN_DIR_B, LOW);
  analogWrite(PIN_PWM_B, 0);
  digitalWrite(PIN_DIR_A, LOW);
  analogWrite(PIN_PWM_A, 0);
  delay(1000);  // Espera 1 segundo quieto antes de iniciar el próximo tramo
}