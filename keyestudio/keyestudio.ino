#include <Arduino.h>

// Pines de motor
#define PIN_DIR_A 4
#define PIN_PWM_A 6
#define PIN_DIR_B 2
#define PIN_PWM_B 5

// Pines del sensor ultrasónico
#define PIN_TRIG 12
#define PIN_ECHO 13

// Pines para enviar/recibir pulsos (conexión a ESP32)
#define PIN_RX 0
#define PIN_TX 1

/**
 * Medir la distancia en centímetros.
 */
int medirDistancia() {
    // apagado del trigger para limpiar lectura anteriores
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);

    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    long tiempo = pulseIn(PIN_ECHO, HIGH, 30000);  // timeout de 30s para estabiizar

    // Si tiempo es 0, no hay eco, por lo tanto el obstaculo está muy lejos
    if (tiempo == 0) {
        return 9999;
    }

    int distancia = tiempo / 59;  // conversión a cm
    return distancia;
}

/**
 * Avanzar a velocidad constante.
 */
void avanzar(int velocidad) {
    digitalWrite(PIN_DIR_A, HIGH);
    digitalWrite(PIN_DIR_B, HIGH);
    analogWrite(PIN_PWM_A, velocidad);
    analogWrite(PIN_PWM_B, velocidad);
}

/**
 * Retroceder a velocidad constante.
 */
void retroceder(int velocidad) {
    digitalWrite(PIN_DIR_A, LOW);
    digitalWrite(PIN_DIR_B, LOW);
    analogWrite(PIN_PWM_A, velocidad);
    analogWrite(PIN_PWM_B, velocidad);
}

/**
 * Frenar completamente.
 */
void frenar() {
    digitalWrite(PIN_DIR_A, LOW);
    digitalWrite(PIN_DIR_B, LOW);
    analogWrite(PIN_PWM_A, 0);
    analogWrite(PIN_PWM_B, 0);
}

/**
 * Girar a la derecha (con cierta velocidad).
 */
void girarDerecha() {
    digitalWrite(PIN_DIR_A, LOW);
    digitalWrite(PIN_DIR_B, HIGH);
    analogWrite(PIN_PWM_A, 50);
    analogWrite(PIN_PWM_B, 200);
}

/**
 * Girar a la izquierda (con cierta velocidad).
 */
void girarIzquierda() {
    digitalWrite(PIN_DIR_A, HIGH);
    digitalWrite(PIN_DIR_B, LOW);
    analogWrite(PIN_PWM_A, 200);
    analogWrite(PIN_PWM_B, 50);
}

/**
 * Enviar pulso de salida.
 */
void enviarPulso() {
    digitalWrite(PIN_TX, HIGH);
    delay(1000);
    digitalWrite(PIN_TX, LOW);
}

/**
 * Configuración.
 */
void setup() {
    // Motores
    pinMode(PIN_DIR_B, OUTPUT);
    pinMode(PIN_PWM_B, OUTPUT);
    pinMode(PIN_DIR_A, OUTPUT);
    pinMode(PIN_PWM_A, OUTPUT);
    analogWrite(PIN_PWM_A, 0);
    analogWrite(PIN_PWM_B, 0);

    // Ultrasónico
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    digitalWrite(PIN_TRIG, LOW);

    // Salida/entrada para interconexion
    pinMode(PIN_TX, OUTPUT);
    pinMode(PIN_RX, INPUT);
    digitalWrite(PIN_TX, LOW);

    // espera a recibir pulso de ESP32 estable
    delay(1000);
    while (true) {
        if (digitalRead(PIN_RX) == HIGH) {
            delay(50);
            if (digitalRead(PIN_RX) == HIGH) {
                break;
            }
        }
        delay(10);
    }

    delay(500);  // margen final de sincronización
}

// Total de plantas a analizar
const int TOTAL_PLANTAS = 3;

// Contador de plantas encontradas hasta el momento
int plantasEncontradas = 0;

/**
 * Ejecución.
 */
void loop() {
    long distancia = medirDistancia();

    if (distancia > 25) {
        avanzar(200);
    } else {
        plantasEncontradas++;
        frenar();

        enviarPulso();

        // espera a recibir pulso de ESP32 estable
        while (digitalRead(PIN_RX) == HIGH) {
            delay(10);
        }
        while (true) {
            if (digitalRead(PIN_RX) == HIGH) {
                delay(50);
                if (digitalRead(PIN_RX) == HIGH) {
                    break;
                }
            }
            delay(10);
        }

        girarDerecha();
        delay(3000);  // Tiempo estimado para girar ~90°

        frenar();
        delay(500);
    }

    if (plantasEncontradas >= TOTAL_PLANTAS) {
        frenar();
        while (true) {}  // loop infinito para "terminar" el programa
    }
}