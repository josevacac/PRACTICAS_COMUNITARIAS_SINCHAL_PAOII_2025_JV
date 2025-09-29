// ==== RECEPTO_LORA_FINAL con debug de CAU (sin cambiar lógica de publicación) ====

// Librerías necesarias
#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <WiFi.h>
#include <PubSubClient.h>

// Pantalla OLED
#ifdef WIRELESS_STICK_V3
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);
#else
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
#endif

// ---------------- LoRa ----------------
#define RF_FREQUENCY 915000000
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 11
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BUFFER_SIZE 64
#define LED_PIN 35

// LoRa variables
bool paqueteRecibido = false;
char rxpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;
int altura;
int16_t rssi, rxSize;
bool lora_idle = true;

// --------------- WiFi ---------------
const char* ssid = "JUNTA DE AGUA";
const char* password = "juntadeagua-2325";
bool estadoWiFi = false;

// -------- MQTT ThingSpeak ----------
const char* mqttServer = "mqtt3.thingspeak.com";
const int   mqttPort   = 1883;
const char* mqttClientID = "CiIDHhwRBwU0Jw4VGhgbPSM";
const char* mqttUsername = "CiIDHhwRBwU0Jw4VGhgbPSM";
const char* mqttPassword = "pAWWWSUe4OEaoHrtH4ppeNIP";
const char* mqttTopic    = "channels/3073798/publish/fields/field1"; // NIVEL
const char* mqttTopic2    = "channels/3073798/publish/fields/field2"; // CAUDAL
WiFiClient espClient;
PubSubClient client(espClient);
bool estadoMQTT = false;

// ----- MQTT Mosquitto (local) -----
const char* mqttServerLocal = "192.168.1.135";
const int   mqttPortLocal   = 1883;
const char* mqttUserLocal   = "miusuario";
const char* mqttPasswordLocal = "brokerraspsinchal2025";
const char* mqttTopicLocal  = "nivel";
WiFiClient espClientLocal;
PubSubClient clientLocal(espClientLocal);
String valorPendiente = "";

// ----- Timeout y reconexión -----
unsigned long ultimoPaqueteMillis = 0;
unsigned long intervaloTimeout = 150000; // 2 min 30 s
bool falloLoraReportado = false;
unsigned long ultimoIntentoWiFi = 0;
const unsigned long intervaloWiFi = 10000;

// ---------------- Pantalla ----------------
void actualizarPantalla(bool loraOK) {
  display.clear();
  if (loraOK) {
    display.drawString(0, 0, "SINCHAL RECEPTOR");
    display.drawString(0, 20, "Altura: " + String(altura) + " cm");
  } else {
    display.drawString(0, 0, "NO HAY SEÑAL LORA");
    display.drawString(0, 20, "Altura: -21 cm");
  }
  display.drawString(0, 35, estadoWiFi ? "WiFi: OK" : "WiFi: FAIL");
  display.drawString(64, 35, estadoMQTT ? "MQTT: OK" : "MQTT: FAIL");
  display.display();
}

// --------------- WiFi ---------------
void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    estadoWiFi = false;
    if (millis() - ultimoIntentoWiFi > intervaloWiFi) {
      Serial.println("Intentando conectar a WiFi...");
      WiFi.begin(ssid, password);
      ultimoIntentoWiFi = millis();
    }
  } else {
    estadoWiFi = true;
  }
}

// --------- ThingSpeak MQTT ----------
void reconnectMQTT() {
  if (estadoWiFi && !client.connected()) {
    Serial.print("Intentando conectar a MQTT ThingSpeak...");
    if (client.connect(mqttClientID, mqttUsername, mqttPassword)) {
      Serial.println("MQTT ThingSpeak conectado.");
      estadoMQTT = true;
    } else {
      Serial.println(String("MQTT ThingSpeak fallo: ") + client.state());
      estadoMQTT = false;
    }
  } else if (client.connected()) {
    estadoMQTT = true;
  } else {
    estadoMQTT = false;
  }
}

// --------- Mosquitto MQTT ----------
void reconnectMQTTLocal() {
  if (estadoWiFi && !clientLocal.connected()) {
    Serial.print("Intentando conectar a MQTT Local...");
    if (clientLocal.connect("HeltecClient", mqttUserLocal, mqttPasswordLocal)) {
      Serial.println("MQTT Local conectado.");
    } else {
      Serial.println(String("MQTT Local fallo: ") + clientLocal.state());
    }
  }
}

// Publicar NIVEL (comportamiento original)
void enviarDatos(int valor) {
  String payload = String(valor);

  // ThingSpeak (field1)
  if (client.connected()) {
    client.publish(mqttTopic, payload.c_str());
    Serial.println("Publicado en ThingSpeak (nivel): " + payload);
    estadoMQTT = true;
  } else {
    estadoMQTT = false;
    Serial.println("NO ");
  }

  delay(2000); // pausa antes de intentar Mosquitto

 
}

void enviarDatos2(int valor) {
  String payload = String(valor);

  // ThingSpeak (field1)
  if (client.connected()) {
    client.publish(mqttTopic2, payload.c_str());
    Serial.println("Publicado en ThingSpeak (caudal): " + payload);
    estadoMQTT = true;
  } else {
    estadoMQTT = false;
    Serial.println("NO ");
  }

  delay(2000); // pausa antes de intentar Mosquitto

 
}

void setup() {
  Serial.begin(115200);
  Mcu.begin();
  pinMode(LED_PIN, OUTPUT);

  display.init();
  display.clear();
  display.display();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  // LoRa RX
  RadioEvents.RxDone = OnRxDone;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

  // Conexiones de red
  WiFi.begin(ssid, password);
  ultimoIntentoWiFi = millis();

  client.setServer(mqttServer, mqttPort);
  clientLocal.setServer(mqttServerLocal, mqttPortLocal);

  Serial.println("Setup completo.");
}

void loop() {
  reconnectWiFi();
  reconnectMQTT();
  reconnectMQTTLocal();
  client.loop();
  clientLocal.loop();

  // Reintento de valor pendiente Mosquitto
  if (!valorPendiente.isEmpty() && clientLocal.connected()) {
    if (clientLocal.publish(mqttTopicLocal, valorPendiente.c_str())) {
      Serial.println("Reintento exitoso (Mosquitto nivel): " + valorPendiente);
      valorPendiente = "";
    }
  }

  // LoRa RX continuo
  if (lora_idle) {
    lora_idle = false;
    Radio.Rx(0);
  }

  // Blink si llegó paquete
  if (paqueteRecibido) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
    paqueteRecibido = false;
  }

  Radio.IrqProcess();

  // Timeout LoRa → enviar -21
  bool loraOK = millis() - ultimoPaqueteMillis <= intervaloTimeout;
  if (!loraOK && !falloLoraReportado) {
    altura = -21;
    enviarDatos(-21);
    Serial.println("Fallo LoRa: enviado -21");
    falloLoraReportado = true;
  }

  actualizarPantalla(loraOK);
  delay(10);
}

// ====== AQUÍ ESTÁ EL CAMBIO (solo logs de debug, sin modificar publicación) ======
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssiVal, int8_t snr) {
  rssi = rssiVal;
  rxSize = size;

  // Copiar payload
  memset(rxpacket, 0, sizeof(rxpacket));
  uint16_t len = (size < (uint16_t)(BUFFER_SIZE - 1)) ? size : (BUFFER_SIZE - 1);
  memcpy(rxpacket, payload, len);
  rxpacket[len] = '\0';

  paqueteRecibido = true;

  // --- DEBUG: ver todo lo que llega tal cual ---
  Serial.printf("RX crudo: \"%s\" (RSSI=%d, SNR=%d)\n", rxpacket, (int)rssi, (int)snr);

  // Caso 1: NIVEL (paquete legado: "123")
  int valorRecibido;
  if (sscanf(rxpacket, "%d", &valorRecibido) == 1) {
    altura = valorRecibido;
    Serial.print("Altura: ");
    Serial.println(altura);
    enviarDatos(altura);                 // <- PUBLICA NIVEL (igual que antes)
    ultimoPaqueteMillis = millis();
    falloLoraReportado = false;
    return;
  }

  // Caso 2: CAUDAL (paquete: "CAU:25") -> SOLO MOSTRAR POR SERIAL (no publicar)
  if (strncmp(rxpacket, "CAU:", 4) == 0) {
    int caudal;
    if (sscanf(rxpacket + 4, "%d", &caudal) == 1) {
      Serial.printf("CAUDAL recibido (debug): %d L/min\n", caudal);
      enviarDatos2(caudal);    
      ultimoPaqueteMillis = millis();    // también resetea timeout LoRa
      falloLoraReportado = false;
      return;
    }
  }

  // Otros paquetes (si hubiera)
  Serial.printf("Paquete no reconocido (debug): %s\n", rxpacket);
}
