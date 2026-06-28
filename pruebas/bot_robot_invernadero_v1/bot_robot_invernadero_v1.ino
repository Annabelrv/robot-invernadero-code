#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "esp_camera.h"


// CONFIGURACIÓN DE CREDENCIALES

const char* ssid = "TU_SSID_WIFI";
const char* password = "TU_PASSWORD_WIFI";
#define BOTtoken = "1234567890:ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // Token de BotFather
#define CHAT_ID = "123456789";                             // Tu ID (IDBot)

// Clientes de red y Telegram
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Tiempos de espera para no saturar los servidores de Telegram
int botRequestDelay = 500;  // Revisa mensajes cada 1 segundo
unsigned long lastTimeBotRan;

// Configuración del hardware físico actual
const int flashPin = 4;  // GPIO 4 es el Flash LED de la ESP32-CAM

// Puntero global para la captura de imagen
camera_fb_t* fb_global = NULL;


// CONFIGURACIÓN DE PINES DE LA CÁMARA (Modelo AI-Thinker)

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
#define V_SYNC_GPIO_NUM 25
#define H_REF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22


// FUNCIÓN PARA ENVIAR FOTO 

String enviarFoto() {
  fb_global = esp_camera_fb_get();
  if (!fb_global) {
    Serial.println("Error: No se pudo capturar la imagen de la cámara");
    return "Error al capturar la foto física.";
  }

  Serial.println("Foto capturada. Conectando directo a la API de Telegram...");

  WiFiClientSecure httpCliente;
  httpCliente.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  if (!httpCliente.connect("api.telegram.org", 443)) {
    Serial.println("Error: No se pudo conectar al servidor api.telegram.org");
    esp_camera_fb_return(fb_global);
    fb_global = NULL;
    return "ERROR_CONEXION";
  }

  String boundary = "FarmaidBoundary";
  String contenido_chat = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(CHAT_ID) + "\r\n";
  String inicio_foto = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"farmaid.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String fin_boundary = "\r\n--" + boundary + "--\r\n";

  long total_longitud = contenido_chat.length() + inicio_foto.length() + fb_global->len + fin_boundary.length();

  // ENVÍO POST HTTP: Añadimos "Connection: close" para liberar el canal al toque
  httpCliente.print("POST /bot" + String(BOTtoken) + "/sendPhoto HTTP/1.1\r\n");
  httpCliente.print("Host: api.telegram.org\r\n");
  httpCliente.print("Connection: close\r\n");  // <-- CLAVE: Fuerza al servidor a cerrar la sesión al terminar
  httpCliente.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  httpCliente.print("Content-Length: " + String(total_longitud) + "\r\n\r\n");

  httpCliente.print(contenido_chat);
  httpCliente.print(inicio_foto);

  uint8_t* fbBuf = fb_global->buf;
  size_t fbLen = fb_global->len;
  for (size_t n = 0; n < fbLen; n += 1024) {
    if (n + 1024 < fbLen) {
      httpCliente.write(fbBuf, 1024);
      fbBuf += 1024;
    } else if (fbLen % 1024 > 0) {
      size_t resto = fbLen % 1024;
      httpCliente.write(fbBuf, resto);
    }
  }

  httpCliente.print(fin_boundary);

  // Espera un instante a que se terminen de vaciar los buffers de salida
  httpCliente.flush();
  httpCliente.stop();  // <--  Cerramos el socket local a la fuerza

  esp_camera_fb_return(fb_global);
  fb_global = NULL;

  Serial.println("¡Datos transmitidos con éxito por HTTP!");
  return "OK";
}

// ==========================================
// GESTIÓN DE MENSAJES RECIBIDOS (LÓGICA)
// ==========================================
void gestionarMensajes(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);

    // Filtro de seguridad: Solo responde a tu ID de Telegram
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Usuario no autorizado", "");
      continue;
    }

    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    Serial.println(text);

    // Comando de inicio / bienvenida
    if (text == "/start") {
      String bienvenida = "¡Hola, " + from_name + "! Bienvenido al Robot Farmaid-Lite.\n";
      bienvenida += "Comandos disponibles:\n\n";
      bienvenida += "/foto      -> ¡Sacar una foto en tiempo real!\n";
      bienvenida += "/flash_on  -> Encender el Flash\n";
      bienvenida += "/flash_off -> Apagar el Flash\n";
      bienvenida += "/estado    -> Ver estado del robot\n";
      // [ZONA DE CAMBIO: Agregar acá los textos de tus nuevos comandos de diagnóstico futuro]

      bot.sendMessage(chat_id, bienvenida, "");
    }

    if (text == "/foto") {
      bot.sendMessage(chat_id, "Capturando imagen, por favor espera...", "");

      // Encendemos el flash un instante para mejorar la iluminación 
      digitalWrite(flashPin, HIGH);
      delay(500);  // Pequeño delay para que la cámara ajuste el brillo

      // Llamamos a nuestra función de foto
      enviarFoto();

      // Apagamos el flash
      digitalWrite(flashPin, LOW);
    }

    // Comando: Encender el Flash
    if (text == "/flash_on") {
      digitalWrite(flashPin, HIGH);
      bot.sendMessage(chat_id, "Flash ENCENDIDO", "");
    }

    // Comando: Apagar el Flash
    if (text == "/flash_off") {
      digitalWrite(flashPin, LOW);
      bot.sendMessage(chat_id, "Flash APAGADO", "");
    }

    // Comando: Consultar Estado
    if (text == "/estado") {
      if (digitalRead(flashPin)) {
        bot.sendMessage(chat_id, "El Flash está ENCENDIDO", "");
      } else {
        bot.sendMessage(chat_id, "El Flash está APAGADO", "");
      }
      // programar que el bot te devuelva si la planta analizada está sana o enferma]
    }
  }
}


// ARRANQUE DEL SISTEMA (SE EJECUTA UNA VEZ)

void setup() {
  Serial.begin(115200);

  // Inicializa el pin del Flash como salida
  pinMode(flashPin, OUTPUT);
  digitalWrite(flashPin, LOW);


 
  // NUEVO: CONFIGURACIÓN E INICIALIZACIÓN DEL HARDWARE DE LA CÁMARA
 
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
  config.pin_vsync = V_SYNC_GPIO_NUM;
  config.pin_href = H_REF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;  // Formato comprimido obligatorio para Telegram

  // Ajustes de calidad e imagen (SVGA 800x600 es ideal para no saturar la RAM)
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // Inicializa físicamente el sensor de la cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error crítico: Falló la inicialización de la cámara 0x%x", err);
    // Si la cámara falla aquí, el programa avisa por el monitor serie pero sigue para dejarte arreglarlo
  } else {
    Serial.println("¡Sensor óptico OV2640 inicializado con éxito!");
  }
  // ------------------------------------------


  // Conexión a la red Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Añade el certificado de seguridad requerido por la API de Telegram
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a la red Wi-Fi...");
  }

  Serial.println("¡Conectado con éxito!");
  Serial.print("Dirección IP local: ");
  Serial.println(WiFi.localIP());

  // [ZONA DE CAMBIO: Aquí inicializarás los pines de comunicación serial hacia el Arduino de la Etapa 2]
}


// BUCLE PRINCIPAL (SE REPITE INFINITAMENTE)

void loop() {
  // Revisa si pasó el tiempo configurado para pedir nuevos mensajes
  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("Mensaje recibido desde Telegram.");
      gestionarMensajes(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  // Aca va la escucha digital para que cuando el Arduino te avise que frenó, la cámara saque la foto de forma autónoma]
}
