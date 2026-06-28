/*
 * ESP32-CAM.
 *
 * Espera la señal del robot,
 * captura una imagen,
 * realiza un análisis cromático
 * y envía foto y diagnóstico a Telegram.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "esp_camera.h"
#include "img_converters.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

// === CONFIGURACIÓN DE CREDENCIALES ===
const char* SSID = "TU_SSID_WIFI";
const char* PASSWORD = "TU_PASSWORD_WIFI";
const char* BOT_TOKEN = "1234567890:ABCDE_fghijklmnopkrstuvwxyz01234567";  // Bot Token (de Botfather)
const char* CHAT_ID = "123456789";                                         // ID individual (@myidbot) autorizado en el bot

// Servidor de la API de Telegram
const char* TELEGRAM_URL = "api.telegram.org";

// Cliente TCP para conectar a Telegram
WiFiClientSecure clientTCP;

// Pines de la cámara (Modelo AI-Thinker)
#define FLASH_LED_PIN 4
#define GPIO_ACK_PIN 12
#define GPIO_SIG_PIN 13

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

/**
 * Configuración inicial de la cámara.
 */
void configurarCamara() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    config.grab_mode = CAMERA_GRAB_LATEST;
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("ESP32-CAM falló inicio con error 0x%x", err);
        delay(1000);
        ESP.restart();
    }
}

/**
 * Conectar camara al Wi Fi
 */
void conectarWifi() {
    WiFi.mode(WIFI_STA);
    Serial.println();
    Serial.print(F("Conectando a la red: "));
    Serial.println(SSID);
    WiFi.begin(SSID, PASSWORD);

    clientTCP.setInsecure();   // no validar SSL para mayor velocidad
    clientTCP.setTimeout(10);  // timeout de lectura (10 ms)

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    Serial.print(F("ESP32-CAM conectada, IP: "));
    Serial.println(WiFi.localIP());
}

/**
 * Captura una imagen y devuelve el framebuffer obtenido.
 * Devuelve NULL si la captura falla.
 */
camera_fb_t* capturarFoto() {
    // Liberar cualquier foto pendiente antes de capturar una nueva
    camera_fb_t* fb_anterior = esp_camera_fb_get();
    if (fb_anterior) {
        esp_camera_fb_return(fb_anterior);
        delay(50);
    }

    camera_fb_t* fb = esp_camera_fb_get();
    return fb;
}

/**
 * Análisis cromático de la foto de la planta, para deducir SANA o ENFERMA.
 */
String ejecutarAnalisisCromatico(camera_fb_t* fb) {
    uint8_t* rgb_buf = (uint8_t*)ps_malloc(fb->width * fb->height * 2);
    if (!rgb_buf) {
        Serial.println("Error de memoria PSRAM.");
        return "Error de memoria en el analisis";
    }

    Serial.println(F("Descomprimiendo JPEG a RGB565 en tiempo real..."));
    if (!jpg2rgb565(fb->buf, fb->len, rgb_buf, JPG_SCALE_NONE)) {
        Serial.println(F("Error al transformar el formato."));
        free(rgb_buf);
        return "Error al procesar la imagen";
    }

    // Contadores para clasificar los píxeles del área central
    unsigned long pixelesVerdes = 0;
    unsigned long pixelesAmarillos = 0;

    // Ancho y alto completo de la captura
    int ancho = fb->width;
    int alto = fb->height;

    // Analizar solamente la zona central de la imagen
    int xInicio = ancho * 0.2;
    int xFin = ancho * 0.8;
    int yInicio = alto * 0.2;
    int yFin = alto * 0.8;

    // Convertir cada píxel RGB565 a RGB888 para compararlo
    for (int y = yInicio; y < yFin; y++) {
        for (int x = xInicio; x < xFin; x++) {
            int indice = (y * ancho + x) * 2;
            uint8_t byte1 = rgb_buf[indice];
            uint8_t byte2 = rgb_buf[indice + 1];
            uint16_t rgb565 = (byte1 << 8) | byte2;

            uint8_t r = ((rgb565 >> 11) & 0x1F) * 255 / 31;
            uint8_t g = ((rgb565 >> 5) & 0x3F) * 255 / 63;
            uint8_t b = (rgb565 & 0x1F) * 255 / 31;

            if (g > r && g > b) {
                pixelesVerdes++;
            } else if (r > b && g > b && abs(r - g) < 40) {
                pixelesAmarillos++;
            }
        }
    }

    free(rgb_buf);

    unsigned long totalClasificados = pixelesVerdes + pixelesAmarillos;

    if (totalClasificados > 0) {
        float ratioEnfermo = ((float)pixelesAmarillos / totalClasificados) * 100.0;
        Serial.printf("Analisis completado. Ratio de daño: %.2f%%\n", ratioEnfermo);

        if (ratioEnfermo > 25.0) {
            return "DIAGNÓSTICO -> ⚠️ PLANTA ENFERMA (Clorosis: " + String(ratioEnfermo, 2) + "%)";
        } else {
            return "DIAGNÓSTICO -> ✅ PLANTA SANA (Daño: " + String(ratioEnfermo, 2) + "%)";
        }
    } else {
        return "No se detecta ninguna hoja en el centro de la camara.";
    }
}

/**
 * Enviar la foto al bot de Telegram.
 */
void enviarFoto(camera_fb_t* fb) {
    if (clientTCP.connect(TELEGRAM_URL, 443)) {
        Serial.println(F("Conexión exitosa. Enviando archivo..."));

        // Construcción manual del formulario multipart
        String head = "--RobotInvernadero\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + String(CHAT_ID) + "\r\n--RobotInvernadero\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
        String tail = "\r\n--RobotInvernadero--\r\n";

        size_t totalLen = fb->len + head.length() + tail.length();

        clientTCP.println("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1");
        clientTCP.println("Host: " + String(TELEGRAM_URL));
        clientTCP.println("Content-Length: " + String(totalLen));
        clientTCP.println("Content-Type: multipart/form-data; boundary=RobotInvernadero");
        clientTCP.println();

        clientTCP.print(head);

        // Enviar la imagen en bloques de 1024 bytes.
        uint8_t* fbBuf = fb->buf;
        size_t fbLen = fb->len;
        for (size_t n = 0; n < fbLen; n = n + 1024) {
            if (n + 1024 < fbLen) {
                clientTCP.write(fbBuf, 1024);
                fbBuf += 1024;
            } else if (fbLen % 1024 > 0) {
                size_t remainder = fbLen % 1024;
                clientTCP.write(fbBuf, remainder);
            }
        }

        clientTCP.print(tail);

        // Liberar recursos de la cámara inmediatamente
        esp_camera_fb_return(fb);

        // Esperar a que Telegram termine de responder para cerrar la conexión
        unsigned long timeout = millis();
        while (clientTCP.connected() && millis() - timeout < 4000) {
            if (clientTCP.available()) {
                clientTCP.read();
                timeout = millis();
            }
        }
        clientTCP.stop();
        Serial.println(F("Foto enviada."));
    } else {
        Serial.println(F("Error: No se pudo conectar a Telegram para enviar la foto"));
        esp_camera_fb_return(fb);
    }
}

/**
 * Enviar diagnostico del analisis cromático a Telegram.
 */
void enviarDiagnostico(String diagnostico) {
    if (clientTCP.connect(TELEGRAM_URL, 443)) {
        Serial.println(F("Conexión exitosa. Enviando texto..."));

        // Codificar caracteres básicos para enviarlos por URL
        diagnostico.replace(" ", "%20");
        diagnostico.replace("\n", "%0A");

        clientTCP.println("GET /bot" + String(BOT_TOKEN) + "/sendMessage?chat_id=" + String(CHAT_ID) + "&text=" + diagnostico + " HTTP/1.1");
        clientTCP.println("Host: " + String(TELEGRAM_URL));
        clientTCP.println("Connection: close");
        clientTCP.println();

        delay(200);
        clientTCP.stop();
        Serial.println(F("Texto del diagnóstico enviado."));
    } else {
        Serial.println(F("Error: No se pudo conectar a Telegram para enviar el diagnostico."));
    }
}

/**
 * Captura foto, analiza cromáticamente y envía diagnostico al Bot.
 */
void capturarAnalizarYEnviar() {
    // Encender el flash para iluminar la planta
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(200);  // tiempo para estabilizar la iluminación

    // Capturar la imagen
    camera_fb_t* fb = capturarFoto();

    // Apagar el flash una vez capturada la imagen
    digitalWrite(FLASH_LED_PIN, LOW);

    if (!fb) {
        Serial.println(F("No se pudo capturar la imagen."));
        return;
    }

    // Analizar la planta
    String diagnostico = ejecutarAnalisisCromatico(fb);

    // Enviar foto y diagnostico
    enviarFoto(fb);
    enviarDiagnostico(diagnostico);
}

/**
 * Configuración.
 */
void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Serial.begin(115200);

    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW);

    pinMode(GPIO_SIG_PIN, INPUT);
    pinMode(GPIO_ACK_PIN, OUTPUT);
    digitalWrite(GPIO_ACK_PIN, LOW);
    delay(500);  // estabilización inicial del GPIO

    configurarCamara();
    conectarWifi();

    // Avisar al robot que la ESP32 terminó de inicializarse
    digitalWrite(GPIO_ACK_PIN, HIGH);
    delay(800);  // dar tiempo al Arduino para detectar el estado HIGH

    Serial.println("ESP32 lista");
}

/**
 * Ejecución: espera solicitudes del robot y procesa cada planta.
 *
 * 1. Espera el pulso enviado por el robot.
 * 2. Indica que la cámara está ocupada (ACK = LOW).
 * 3. Captura una imagen de la planta.
 * 4. Realiza el análisis cromático.
 * 5. Envía la foto y el diagnóstico.
 * 6. Indica que volvió a quedar disponible (ACK = HIGH).
 */
void loop() {
    // Esperar el pulso del robot
    if (digitalRead(GPIO_SIG_PIN) == HIGH) {
        delay(30);  // anti rebote

        // Confirmar que no fue un rebote.
        if (digitalRead(GPIO_SIG_PIN) == HIGH) {
            // Esperar a que el robot termine de enviar el pulso
            while (digitalRead(GPIO_SIG_PIN) == HIGH) {
                delay(10);
            }

            // Avisar que la cámara está ocupada
            // El robot queda esperando hasta que ACK vuelva a HIGH
            digitalWrite(GPIO_ACK_PIN, LOW);

            // Captura de foto, analisis cromatico y envío de resultado
            capturarAnalizarYEnviar();

            // Avisar que la cámara volvió a quedar disponible
            digitalWrite(GPIO_ACK_PIN, HIGH);
            Serial.println("Proceso terminado, ESP32 lista");
        }
    }
}
