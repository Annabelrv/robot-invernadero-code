# Robot móvil para monitoreo de plantas

Proyecto desarrollado con un robot móvil Keyestudio y una ESP32-CAM para recorrer un invernadero, capturar imágenes de plantas, realizar un análisis cromático básico y enviar los resultados a Telegram.

## Estructura del proyecto

```text
esp32/
├── esp32.ino
└── README.md

keyestudio/
├── keyestudio.ino
└── README.md

pruebas/
└── (programas utilizados durante el desarrollo y las pruebas)
```

## Funcionamiento

El robot avanza por el recorrido utilizando un sensor ultrasónico para detectar plantas.

Cuando encuentra una planta:

1. El robot se detiene.
2. Envía una señal a la ESP32-CAM.
3. La ESP32 captura una fotografía.
4. Realiza un análisis cromático para estimar el estado de la planta.
5. Envía la foto y el diagnóstico a Telegram.
6. La ESP32 indica que finalizó el proceso.
7. El robot continúa su recorrido hasta completar la cantidad de plantas configurada.

## Protocolo de comunicación

La comunicación entre ambos dispositivos se realiza mediante dos señales digitales:

* **Robot → ESP32:** solicitud de captura.
* **ESP32 → Robot:** confirmación de disponibilidad (ACK).

Este mecanismo sincroniza ambos dispositivos sin utilizar comunicación serie.
