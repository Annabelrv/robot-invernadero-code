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

// Credenciales para Wi Fi y Telegram
const char* SSID = "TU_SSID_WIFI";                                         // Nombre de la red Wi Fi
const char* PASSWORD = "TU_PASSWORD_WIFI";                                 // Contraseña de Wi Fi
const char* BOT_TOKEN = "1234567890:ABCDE_fghijklmnopkrstuvwxyz01234567";  // Token del bot de Telegram (de BotFather)
const char* CHAT_ID = "123456789";                                         // ID individual (@myidbot) autorizado en el bot

// URL de la API de Telegram (para enviar mensajes)
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
 * Registros, buffers y formato del sensor óptico OV2640.
 */
void configurarCamara() {
    // Variable de configuracion
    camera_config_t config;

    // Pines
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

    // Configuraciones de captura (sensor optico, formato, calidad, buffers, etc.)
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;

    // Inicialización de la camara
    esp_err_t err = esp_camera_init(&config);
    // Si no inicializa, se reporta y reinicia
    if (err != ESP_OK) {
        Serial.printf("ESP32-CAM falló inicio con error 0x%x", err);
        delay(1000);
        ESP.restart();
    }
}

/**
 * Conexión de la placa al Wi Fi
 */
void conectarWifi() {
    // Selecciona el modo de Wi Fi
    WiFi.mode(WIFI_STA);

    // Mensaje de "conexión en curso" en el monitor serial
    Serial.println();
    Serial.print(F("Conectando a la red: "));
    Serial.println(SSID);

    // Conecta a la red Wi Fi con su contraseña
    WiFi.begin(SSID, PASSWORD);

    // Configuraciones del cliente TCP/IP
    clientTCP.setInsecure();   // no validar SSL para mayor velocidad
    clientTCP.setTimeout(10);  // timeout de lectura de 10 ms

    // Espera a que la conexión esté lista (mostrando puntos suspensivos)
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    // Mensaje en monitor de "conexión exitosa"
    Serial.println();
    Serial.print(F("ESP32-CAM conectada, IP: "));
    Serial.println(WiFi.localIP());
}

/**
 * Captura una foto y devuelve la imagen obtenida.
 * Devuelve NULL si la captura falla.
 */
camera_fb_t* capturarFoto() {
    // Borra cualquier foto pendiente antes de capturar una nueva para evitar fotos falsas
    // Intenta capturar una foto
    camera_fb_t* fb_anterior = esp_camera_fb_get();
    // Si hay captura, la borra y sigue
    if (fb_anterior) {
        esp_camera_fb_return(fb_anterior);
        delay(50);
    }

    // Captura una nueva foto y la retorna
    camera_fb_t* fb = esp_camera_fb_get();
    return fb;
}

/**
 * Análisis cromático de la foto de la planta con calibración física.
 * Filtra el fondo mediante saturación y clasifica usando balance directo de canales.
 */
String ejecutarAnalisisCromatico(camera_fb_t* fb) {
    // Reserva de memoria PSRAM externa para la imagen descomprimida en RGB565
    uint8_t* rgb_buf = (uint8_t*)ps_malloc(fb->width * fb->height * 2);
    // Si falla la memoria, se reporta y termina
    if (!rgb_buf) {
        Serial.println("Error de memoria PSRAM.");
        return "Error de memoria en el analisis";
    }

    // Mensaje en monitor serial para marcar el inicio
    Serial.println(F("Descomprimiendo JPEG a RGB565..."));

    // Descomprime el JPEG para obtener una imagen RGB565,
    // con los canales RGB (rojo, verde, azul) disponibles
    if (!jpg2rgb565(fb->buf, fb->len, rgb_buf, JPG_SCALE_NONE)) {
        // Si falla, reporta, libera la memoria y termina
        Serial.println(F("Error al transformar el formato."));
        free(rgb_buf);
        return "Error al procesar la imagen";
    }

    // Contadores de pixeles validos totales y de cada color, para el cálculo
    unsigned long pixelesVerdes = 0;
    unsigned long pixelesAmarillos = 0;
    unsigned long pixelesValidos = 0;

    // Dimensiones de la captura
    int ancho = fb->width;
    int alto = fb->height;

    // Región de interés (ROI) al 60% central para evitar bordes externos,
    // sacando 20% de los cuatros extremos
    int xInicio = ancho * 0.2;
    int xFin = ancho * 0.8;
    int yInicio = alto * 0.2;
    int yFin = alto * 0.8;

    // Barrido píxel por píxel (matriz) limitado dentro de la ROI (el 60% central)
    for (int y = yInicio; y < yFin; y++) {
        for (int x = xInicio; x < xFin; x++) {
            // Calcula el índice de memoria: cada píxel en RGB565 ocupa exactamente 2 bytes
            int index = (y * ancho + x) * 2;

            // Reconstruye de dato binario de 16 bits juntando el byte alto y el byte bajo
            uint8_t byte1 = rgb_buf[index];
            uint8_t byte2 = rgb_buf[index + 1];
            uint16_t rgb565 = (byte1 << 8) | byte2;

            // Decodifica imagen RGB565 a RGB888 (escalamiento de canales a rango 0-255)
            uint8_t r = ((rgb565 >> 11) & 0x1F) * 255 / 31; // 5 bits para el rojo
            uint8_t g = ((rgb565 >> 5) & 0x3F) * 255 / 63;  // 6 bits para el verde (dominante)
            uint8_t b = (rgb565 & 0x1F) * 255 / 31;         // 5 bits para el azul

            // Normaliza los valores 0-255 a rango "porcentual" entre 0 y 1 (100% -> 255)
            float rf = r / 255.0;
            float gf = g / 255.0;
            float bf = b / 255.0;

            // Calcula extremos cromáticos para aislar la pureza del píxel
            float maxVal = fmaxf(rf, fmaxf(gf, bf));
            float minVal = fminf(rf, fminf(gf, bf));
            float delta = maxVal - minVal;
            
            // Saca el componente de saturación (S) del espacio HSV
            float s = (maxVal == 0) ? 0 : (delta / maxVal);

            // Si el píxel está muy lavado por luz ambiente o es gris/sombra, se descarta
            if (s < 0.15) {
                continue;
            }
            // Si no se descarta, es un píxel válido (suma contador) y continúa
            pixelesValidos++;

            // Clasificación:
            // Si el verde supera al rojo por más de un 15%, el píxel se clasifica como verde
            if (g > (r * 1.15)) {
                pixelesVerdes++;
            }
            // Si el rojo está muy cerca o empata con el verde, el píxel se clasifica como amarillo
            else if (r >= (g * 0.75)) {
                pixelesAmarillos++;
            }
        }
    }

    // Libera la memoria de inmediato para evitar bloqueos en la ESP32-CAM
    free(rgb_buf);

    // Si no hay píxeles válidos se descarta análisis
    if (pixelesValidos == 0) {
        return "No se detecta vegetacion util en la imagen.";
    }

    // Calcula porcentajes de verde y amarillo para diagnosticar
    float ratioAmarillo = ((float)pixelesAmarillos / pixelesValidos) * 100.0;
    float ratioVerde = ((float)pixelesVerdes / pixelesValidos) * 100.0;

    // Mensaje en monitor serie con porcentajes de verde y amarillo
    Serial.printf("Verde real: %.2f%% | Amarillo detectado: %.2f%%\n", ratioVerde, ratioAmarillo);

    // Diagnostico final según el umbral del 15%
    if (ratioAmarillo > 15.0) {
        return "DIAGNÓSTICO -> ⚠️ PLANTA ENFERMA (Clorosis: " + String(ratioAmarillo, 2) + "%)";
    } else {
        return "DIAGNÓSTICO -> ✅ PLANTA SANA (Daño: " + String(ratioAmarillo, 2) + "%)";
    }
}

/**
 * Envía la foto al bot de Telegram.
 */
void enviarFoto(camera_fb_t* fb) {
    // Intenta conectarse a Telegram por HTTPS (puerto 443)
    if (clientTCP.connect(TELEGRAM_URL, 443)) {
        // Mensaje en monitor de "conexión exitosa"
        Serial.println(F("Conexión exitosa. Enviando archivo..."));

        // Construcción manual del formulario multipart:
        // Encabezado para adjuntar el ID de chat y la imagen
        String head = "--RobotInvernadero\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + String(CHAT_ID) + "\r\n--RobotInvernadero\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
        // Delimitador de cierre del mensaje
        String tail = "\r\n--RobotInvernadero--\r\n";

        // Calcula el tamaño total del mensaje: tamaño de imagen + largo del texto del formulario
        size_t totalLen = fb->len + head.length() + tail.length();

        // Sube archivos al endpoint de Telegram /sendPhoto utilizando el método POST de HTTP
        clientTCP.println("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1");
        clientTCP.println("Host: " + String(TELEGRAM_URL));
        clientTCP.println("Content-Length: " + String(totalLen));
        clientTCP.println("Content-Type: multipart/form-data; boundary=RobotInvernadero");
        clientTCP.println();

        // Envía primero el encabezado
        clientTCP.print(head);

        // Variables de control para segmentar los datos binarios de la imagen
        uint8_t* fbBuf = fb->buf;
        size_t fbLen = fb->len;

        // Envía el binario de la imagen en bloques de 1024 bytes.
        for (size_t n = 0; n < fbLen; n = n + 1024) {
            // Mientras encuentre 1024 bytes, envía bloque completo
            if (n + 1024 < fbLen) {
                clientTCP.write(fbBuf, 1024);
                fbBuf += 1024;
            // Al final, envía el resto (menos de 1024 bytes)
            } else if (fbLen % 1024 > 0) {
                size_t remainder = fbLen % 1024;
                clientTCP.write(fbBuf, remainder);
            }
        }

        // Termina enviando el delimitador del mensaje
        clientTCP.print(tail);

        // Libera la memoria de la cámara inmediatamente
        esp_camera_fb_return(fb);

        // Esperaa a que Telegram termine de confirmar que recibió el mensaje (máximo por 4s)
        unsigned long timeout = millis();
        while (clientTCP.connected() && millis() - timeout < 4000) {
            if (clientTCP.available()) {
                clientTCP.read();
                timeout = millis();
            }
        }
        // Cuando Telegram confirma, se cierra la conexión y se informa en monitor
        clientTCP.stop();
        Serial.println(F("Foto enviada."));
    
    } else { // Si falla la conexión
        // Reporte en el monitor serial
        Serial.println(F("Error: No se pudo conectar a Telegram para enviar la foto"));
        // Libera la memoria de la cámara
        esp_camera_fb_return(fb);
    }
}

/**
 * Enviar diagnostico del analisis cromático a Telegram.
 */
void enviarDiagnostico(String diagnostico) {
    // Intenta conectarse a Telegram por HTTPS (puerto 443)
    if (clientTCP.connect(TELEGRAM_URL, 443)) {
        // Mensaje en monitor de "conexión exitosa"
        Serial.println(F("Conexión exitosa. Enviando texto..."));

        // Codifica caracteres básicos para enviarlos por URL
        diagnostico.replace(" ", "%20");
        diagnostico.replace("\n", "%0A");

        // Envía el mensaje al endpoint /sendMessage por método GET de HTTP, con el ID de chat y el texto del diagnóstico
        clientTCP.println("GET /bot" + String(BOT_TOKEN) + "/sendMessage?chat_id=" + String(CHAT_ID) + "&text=" + diagnostico + " HTTP/1.1");
        clientTCP.println("Host: " + String(TELEGRAM_URL));
        clientTCP.println("Connection: close");
        clientTCP.println();

        // Margen para asegurar que se envió
        delay(200);
        // Cierra la conexión y reporta el envío
        clientTCP.stop();
        Serial.println(F("Texto del diagnóstico enviado."));

    } else {// Si falla la conexión, se reporta el error
        Serial.println(F("Error: No se pudo conectar a Telegram para enviar el diagnostico."));
    }
}

/**
 * Captura foto, analiza cromáticamente y envía diagnostico al bot.
 */
void capturarAnalizarYEnviar() {
    // Prende el flash para iluminar la planta
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(200);  // tiempo para estabilizar la iluminación

    // Captura la imagen
    camera_fb_t* fb = capturarFoto();

    // Apaga el flash una vez capturada
    digitalWrite(FLASH_LED_PIN, LOW);

    // Si no hay captura, reporta el error en monitor y termina
    if (!fb) {
        Serial.println(F("No se pudo capturar la imagen."));
        return;
    }

    // Si hay captura, ejecuta el análisis cromático
    String diagnostico = ejecutarAnalisisCromatico(fb);

    // Envia la foto y el diagnóstico
    enviarFoto(fb);
    enviarDiagnostico(diagnostico);
}

/**
 * Configuración de la placa ESP32.
 */
void setup() {
    // Desactiva el detector físico de Brownout (caída de tensión) del microcontrolador. 
    // Evita reinicios no deseados causados por picos de consumo eléctrico al activar la antena Wi-Fi o el Flash.
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // Inicia el monitor serial a los 115200 baudios de ESP32
    Serial.begin(115200);

    // Comportamiento del pin del flash, iniciando apagado
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW);

    // Comportamiento del pin de entrada (conectado a Keyestudio con baja de tensión)
    pinMode(GPIO_SIG_PIN, INPUT);

    // Comportamiento del pin de salida (conectado a Keyestudio, para avisar "estoy lista")
    pinMode(GPIO_ACK_PIN, OUTPUT);
    // Pin de salida inicialmente apagado (no esta lista aún)
    digitalWrite(GPIO_ACK_PIN, LOW);

    // Delay de seguridad para estabilizar las señales de los procesadores
    delay(500);

    // Configura la cámara OV2640
    configurarCamara();
    // Conecta la placa al Wi Fi
    conectarWifi();

    // Activa el pin de salida para avisar a Keyestudio que terminó de configurar y está lista
    digitalWrite(GPIO_ACK_PIN, HIGH);
    // Margen de espera para garantizar que Keyestudio detecte el cambio en la señal
    delay(800);

    Serial.println("ESP32 lista");
}

/**
 * Ejecución ESP32: espera solicitudes del robot y analiza cada planta.
 *
 * 1. Espera el pulso enviado por Keyestudio.
 * 2. Indica que la cámara está ocupada (señal de salida = LOW).
 * 3. Captura la imagen de la planta.
 * 4. Realiza el análisis cromático.
 * 5. Envía la foto y el diagnóstico.
 * 6. Indica que volvió a quedar disponible (señal de salida = HIGH).
 */
void loop() {
    // Espera el pulso de Keyestudio
    if (digitalRead(GPIO_SIG_PIN) == HIGH) {
        delay(30);  // anti rebote

        // Confirma que no fue un rebote leyendo nuevamente
        if (digitalRead(GPIO_SIG_PIN) == HIGH) {
            // Espera a que Keyestudio termine de enviar el pulso
            while (digitalRead(GPIO_SIG_PIN) == HIGH) {
                delay(10);
            }

            // Avisa que la cámara está ocupada, desactivando la señal de salida
            // Keyestudio queda esperando hasta que la señal vuelva a HIGH
            digitalWrite(GPIO_ACK_PIN, LOW);

            // Captura la foto, ejecuta análisis cromático y envía resultado
            capturarAnalizarYEnviar();

            // Avisa que la cámara volvió a quedar disponible, activando señal de salida
            digitalWrite(GPIO_ACK_PIN, HIGH);
            Serial.println("Proceso terminado, ESP32 lista");
        }
    }
}
