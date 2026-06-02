#include "esp_camera.h"
#include <WiFi.h>

// === CONFIGURACIÓN DE TU RED WI-FI ===
const char* ssid = "TU_NOMBRE_DE_WIFI";
const char* password = "TU_CONTRASEÑA";

// === CONFIGURACIÓN DE PINES (Modelo Estándar AI-Thinker) ===
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void setup() {
  Serial.begin(115200);
  
  // 1. Conectar al Wi-Fi de forma simple
  WiFi.begin(ssid, password);
  Serial.print("Conectando a Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡Conectado exitosamente!");

  // 2. Configuración segura de la cámara para evitar caídas de RAM
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  
  // Sencillo y ligero: Formato nativo a baja resolución
  config.pixel_format = PIXFORMAT_RGB565; 
  config.frame_size = FRAMESIZE_QQVGA;    // 160x120 píxeles (Ideal para no saturar la placa)
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error al iniciar cámara: 0x%x", err);
    while(true); // Detener si hay error de hardware
  }
}

void loop() {
  // Capturar el frame
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Fallo en captura de cámara");
    delay(5000);
    return;
  }

  unsigned long pixelesVerdes = 0;
  unsigned long pixelesAmarillos = 0;

  int ancho = fb->width;
  int alto = fb->height;
  
  // Tu ventana central del 60% para evitar el fondo del salón de clases
  int xInicio = ancho * 0.2;
  int xFin = ancho * 0.8;
  int yInicio = alto * 0.2;
  int yFin = alto * 0.8;

  for (int y = yInicio; y < yFin; y++) {
    for (int x = xInicio; x < xFin; x++) {
      
      int indice = (y * ancho + x) * 2;
      uint8_t byte1 = fb->buf[indice];
      uint8_t byte2 = fb->buf[indice + 1];
      uint16_t rgb565 = (byte1 << 8) | byte2;

      // Descompresión inmediata a RGB estándar
      uint8_t r = ((rgb565 >> 11) & 0x1F) * 255 / 31;
      uint8_t g = ((rgb565 >> 5) & 0x3F) * 255 / 63;
      uint8_t b = (rgb565 & 0x1F) * 255 / 31;

      // Clasificación de color
      if (g > r && g > b) {
        pixelesVerdes++;
      } 
      else if (r > b && g > b && abs(r - g) < 40) {
        pixelesAmarillos++;
      }
    }
  }

  unsigned long totalClasificados = pixelesVerdes + pixelesAmarillos;
  
  if (totalClasificados > 0) {
    float ratioEnfermo = ((float)pixelesAmarillos / totalClasificados) * 100.0;
    
    Serial.printf("\n--- NUEVA LECTURA ---\n");
    Serial.printf("Porcentaje de daño foliar: %.2f%%\n", ratioEnfermo);

    if (ratioEnfermo > 25.0) {
      Serial.println("DIAGNÓSTICO -> PLANTA ENFERMA (Clorosis)");
      // AQUÍ ENVIARÍAS LA ALERTA POR INTERNET
    } else {
      Serial.println("DIAGNÓSTICO -> PLANTA SANA");
    }
  } else {
    Serial.println("No se detecta ninguna hoja en el centro de la cámara.");
  }

  esp_camera_fb_return(fb); // Liberar buffer siempre
  
  delay(5000); // Repetir el análisis cada 5 segundos
}
