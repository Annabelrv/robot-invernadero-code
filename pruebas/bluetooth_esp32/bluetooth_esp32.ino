#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

const int PIN_FLASH = 4;
bool dispositivoEncontrado = false;
BLEAdvertisedDevice robotDispositivo; // Cambiado a objeto directo para evitar punteros sueltos
bool conectado = false;

// La clase para capturar los dispositivos encontrados se mantiene igual
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
      Serial.print("[SCAN] Dispositivo encontrado: ");
      Serial.println(advertisedDevice.toString().c_str());
      
      if (advertisedDevice.haveName()) {
        String nombre = advertisedDevice.getName().c_str();
        // Buscamos coincidencias con los nombres típicos de Keyestudio
        if (nombre.indexOf("KS") != -1 || nombre.indexOf("Keye") != -1 || nombre.indexOf("BT") != -1) {
          Serial.println("[MATCH] ¡Encontramos el BLE del Robot Keyestudio!");
          
          robotDispositivo = advertisedDevice;
          dispositivoEncontrado = true;
          BLEDevice::getScan()->stop(); // Detenemos el escaneo usando la clase global
        }
      }
    }
};

bool conectarAlRobot() {
    Serial.print("[CONN] Intentando conectar a: ");
    Serial.println(robotDispositivo.getAddress().toString().c_str());
    
    BLEClient* pClient = BLEDevice::createClient();
    Serial.println("[CONN] Cliente creado.");

    if (pClient->connect(&robotDispositivo)) {
      Serial.println("[OK] ¡Conectado exitosamente a la placa del robot!");
      conectado = true;
      return true;
    } else {
      Serial.println("[ERROR] No se pudo concretar la conexión física.");
      return false;
    }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_FLASH, OUTPUT);
  digitalWrite(PIN_FLASH, LOW);
  
  Serial.println("[SYSTEM] Iniciando Escáner BLE Moderno en ESP32-CAM...");
  BLEDevice::init("");
  
  // NUEVA SINTAXIS: Obtenemos el escáner e iniciamos pasándole los callbacks directo
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  
  // En las nuevas versiones, se inicia el escaneo pasando la función callback aquí mismo:
  pBLEScan->start(5, false); 
}

void loop() {
  if (dispositivoEncontrado && !conectado) {
    conectarAlRobot();
  }
  
  // Si pasaron 10 segundos y no encontramos nada, reinstanciamos el escaneo limpio
  if (!dispositivoEncontrado && (millis() % 10000 < 20)) {
    Serial.println("[RE-SCAN] Buscando dispositivos BLE nuevamente...");
    BLEDevice::getScan()->start(5, false);
  }
  
  delay(10);
}