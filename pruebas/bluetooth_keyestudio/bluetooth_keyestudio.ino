#include <Arduino.h>

void setup() {
  // Velocidad oficial según los proyectos 15, 16 y 17 del manual
  Serial.begin(9600); 
}

void loop() {
  // Escupimos la 'F' al puerto serial, el módulo esclavo la transmite automáticamente
  Serial.print('F'); 
  
  // Esperamos 3 segundos antes de repetir
  delay(3000); 
}