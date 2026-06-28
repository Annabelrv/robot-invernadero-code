/*
 * Robot móvil Keyestudio.
 *
 * Recorre el invernadero,
 * detecta plantas mediante un sensor ultrasónico
 * y coordina la captura de imágenes con la ESP32-CAM.
 */
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

// Total de plantas a analizar
const int TOTAL_PLANTAS = 3;

// Contador de plantas encontradas hasta el momento
int plantasEncontradas = 0;

/**
 * Medir la distancia en centímetros.
 */
int medirDistancia() {
    // Apagar el trigger para limpiar lecturas anteriores
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);

    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    // Esperar el eco hasta 30ms
    long tiempo = pulseIn(PIN_ECHO, HIGH, 30000);

    // Si no hubo eco (pulseIn -> 0) se considera que el obstaculo está muy lejos
    if (tiempo == 0) {
        return 9999;
    }

    // Convertir el tiempo de eco a centímetros
    int distancia = tiempo / 59;
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
    
    // Esperar hasta que la ESP32 indique que está lista
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

    // Margen final para asegurar sincronización de ambos dispositivos
    delay(500);
}

/**
 * Ejecución: recorrido del robot y sincronización con ESP32.
 *
 * 1. Avanza hasta detectar una planta con el sensor ultrasónico.
 * 2. Se detiene y envía un pulso a la ESP32.
 * 3. Espera mientras la ESP32 captura, analiza y envía los resultados.
 * 4. Cuando la ESP32 vuelve a estar disponible, gira para continuar el recorrido.
 * 5. Finaliza después de analizar la cantidad de plantas configurada.
 */
void loop() {
    long distancia = medirDistancia();

    if (distancia > 25) {
        avanzar(200);
    } else {
        plantasEncontradas++;
        frenar();

        enviarPulso();

        // Esperar a que la ESP32 termine:
        // captura -> análisis -> envío a Telegram
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
        while (true) {
        }  // loop infinito para "terminar" el programa
    }
}