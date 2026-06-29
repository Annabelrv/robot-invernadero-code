/*
 * Robot móvil Keyestudio.
 *
 * Recorre el invernadero,
 * detecta plantas mediante un sensor ultrasónico
 * y coordina la captura de imágenes con la ESP32-CAM.
 */
#include <Arduino.h>

// Pines de los motores
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
 * Mide la distancia en centímetros.
 */
int medirDistancia() {
    // Apaga el trigger para limpiar lecturas anteriores
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);

    // Activa el trigger por 10µs para enviar señal
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    // Esperaa el eco (hasta 30ms para no colgarse)
    long tiempo = pulseIn(PIN_ECHO, HIGH, 30000);

    // Si no hubo eco (pulseIn -> 0) se considera que el obstaculo está muy lejos
    if (tiempo == 0) {
        return 9999;  // distancia grande para representar "muy lejos"
    }

    // Convierte el tiempo de eco a centímetros y retorna
    int distancia = tiempo / 59;
    return distancia;
}

/**
 * Avanza a velocidad constante.
 * 
 * La velocidad corresponde al valor PWM, en el rango de 0-255.
 * Con los pines DIR en HIGH (avanza), cuanto menor sea el PWM más rápido girará el motor.
 */
void avanzar(int velocidad) {
    // DIR de ambos lados en HIGH -> avanzan
    digitalWrite(PIN_DIR_A, HIGH);
    digitalWrite(PIN_DIR_B, HIGH);

    // PWM de ambos a la misma velocidad
    analogWrite(PIN_PWM_A, velocidad);
    analogWrite(PIN_PWM_B, velocidad);
}

/**
 * Frena completamente.
 */
void frenar() {
    // PWM de ambos lados en 0 para que los motores no giren
    analogWrite(PIN_PWM_A, 0);
    analogWrite(PIN_PWM_B, 0);
}

/**
 * Gira a la derecha.
 * 
 * La velocidad corresponde al valor PWM, en el rango de 0-255.
 * Mientras un lado avanza (HIGH) con valor alto de PWM, el otro retrocede con valor bajo.
 */
void girarDerecha(int velocidad) {
    // Lado izquierdo:
    // DIR en LOW para retroceder
    digitalWrite(PIN_DIR_A, LOW);
    // PWM en valor bajo (se acerca a 0)
    analogWrite(PIN_PWM_A, 255 - velocidad);

    // Lado derecho:
    // DIR en LOW para retroceder
    digitalWrite(PIN_DIR_B, HIGH);
    // PWM en valor alto (se acerca a 255)
    analogWrite(PIN_PWM_B, velocidad);
}

/**
 * Envia pulso de salida para activar la ESP32.
 */
void enviarPulso() {
    // Prende el PIN TX por 1 segundo (conectado a ESP32, con baja de tension) 
    digitalWrite(PIN_TX, HIGH);
    delay(1000);
    digitalWrite(PIN_TX, LOW);
}

/**
 * Configuración de la placa Keyestudio.
 */
void setup() {
    // Comportamiento de los DIR y PWM de los motores
    pinMode(PIN_DIR_B, OUTPUT);
    pinMode(PIN_PWM_B, OUTPUT);
    pinMode(PIN_DIR_A, OUTPUT);
    pinMode(PIN_PWM_A, OUTPUT);
    // PWM inicial en 0 (motores detenidos)
    analogWrite(PIN_PWM_A, 0);
    analogWrite(PIN_PWM_B, 0);

    // Comportamiento de los pines del Ultrasónico
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    // Trigger inicialmente apagado
    digitalWrite(PIN_TRIG, LOW);

    // Comportamiento del pin de salida a ESP3
    pinMode(PIN_TX, OUTPUT);
    // Salida inicialmente inactiva
    digitalWrite(PIN_TX, LOW);

    // Comportamiento del pin de entrada desde ESP32
    pinMode(PIN_RX, INPUT);

    // Tiempo de espera fijo para estabilizar
    delay(1000);

    // Espera hasta que la ESP32 indique que está lista,
    // dado que a ESP32 le toma un tiempo configurar y conectarse a Wi Fi
    while (true) {
        // Lee el pin de entrada, para ver si ESP32 respondió
        if (digitalRead(PIN_RX) == HIGH) {
            // Espera un instante y repite para evitar falsas lecturas
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
 * Ejecución Keyestudio: recorrido del robot y sincronización con ESP32.
 *
 * 1. Avanza hasta detectar una planta con el sensor ultrasónico.
 * 2. Se detiene y envía un pulso a la ESP32.
 * 3. Espera mientras la ESP32 captura, analiza y envía los resultados.
 * 4. Cuando la ESP32 vuelve a estar disponible, gira para continuar el recorrido.
 * 5. Finaliza después de analizar la cantidad de plantas configurada.
 */
void loop() {
    // Mide distancia al próximo obstaculo
    long distancia = medirDistancia();

    // Si no hay obstaculos a 25 cm o menos, avanza
    if (distancia > 25) {
        avanzar(200);

    // Sino (obstaculo a 25cm)
    } else {
        // Encontró una planta, suma 1 al contador
        plantasEncontradas++;

        // Frena los motores
        frenar();

        // Envía pulso para activar la ESP32
        enviarPulso();

        // Esperar a que la ESP32 termine:
        // captura -> análisis -> envío a Telegram
        while (true) {
            // Lee el pin de entrada, para ver si ESP32 respondió
            if (digitalRead(PIN_RX) == HIGH) {
                // Espera un instante y repite para evitar falsas lecturas
                delay(50);
                if (digitalRead(PIN_RX) == HIGH) {
                    break;
                }
            }
            delay(10);
        }

        // Gira a la derecha, aproximadamente 90° (delay estimado según velocidad)
        girarDerecha(180);
        delay(1500);

        // Frena el giro y espera antes de continuar
        frenar();
        delay(500);
    }

    // Si encontró la cantidad de plantas total debe terminar
    if (plantasEncontradas >= TOTAL_PLANTAS) {
        // Frena y entra en while infinito para no hacer nada mas
        frenar();
        while (true) {}
    }
}