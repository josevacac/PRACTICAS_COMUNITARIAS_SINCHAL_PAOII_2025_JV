// ===================== TRANSMISOR_LORA_FINAL_AGREGADO_DEBUG.ino =====================
#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"

// ---------- OLED ----------
#ifdef WIRELESS_STICK_V3
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);
#else
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
#endif

// ---------- Sensor de nivel ----------
#define LEVEL_SENSOR 4

// ---------- LoRa ----------
#define RF_FREQUENCY           915000000
#define TX_OUTPUT_POWER        21
#define LORA_BANDWIDTH         0       // 125 kHz
#define LORA_SPREADING_FACTOR  11
#define LORA_CODINGRATE        1       // 4/5
#define LORA_PREAMBLE_LENGTH   8
#define LORA_SYMBOL_TIMEOUT    0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON   false
#define TX_TIMEOUT_VALUE       8000
#define BUFFER_SIZE            128

static RadioEvents_t RadioEvents;
bool lora_idle = true;

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

// ---------- Variables nivel (tus mismas constantes) ----------
int   alturaMaxima        = 500;
float valorsinsumergir    = 0.60;
int   portValue;
float nivelVoltios;
float diferenciaVoltaje;
float valorMaximoAbsoluto;
int   alturaActual;

#define NUM_LECTURAS 6
int lecturas[NUM_LECTURAS];

// ====== STUBS / Sensores extra (reemplaza con lecturas reales cuando los conectes) ======
int leerCaudalLPM(){
  static int v = 0; v = (v + 3) % 50;    // simula 0..49 L/min
  return v;
}
float leerPresionPSI(){
  static float p = 20.0; p += 0.5; if (p > 60.0) p = 20.0; // simula 20..60 psi
  return p;
}

// ====== Estado de TX para logs claros ======
enum TxKind { TX_NONE=0, TX_NIVEL=1, TX_CAUDAL=2 };
volatile TxKind lastTx = TX_NONE;

// ====== Caudal recibido desde NodoCaudal ======
volatile int latest_caudal_lpm = -1;
volatile bool have_caudal = false;

// ====== Helpers IQR ======
void ordenarArray(int *arr, int n) {
  for (int i = 0; i < n-1; i++) {
    for (int j = 0; j < n-i-1; j++) {
      if (arr[j] > arr[j+1]) { int tmp = arr[j]; arr[j] = arr[j+1]; arr[j+1] = tmp; }
    }
  }
}
float calcularIQRMedia(int *arr, int n) {
  ordenarArray(arr, n);
  float Q1 = (arr[1] + arr[2]) / 2.0;
  float Q3 = (arr[3] + arr[4]) / 2.0;
  float IQR = Q3 - Q1;
  float LI = Q1 - 1.5 * IQR;
  float LS = Q3 + 1.5 * IQR;
  long suma = 0; int count = 0;
  for (int i = 0; i < n; i++) if (arr[i] >= LI && arr[i] <= LS) { suma += arr[i]; count++; }
  if (count == 0) return -1;
  return (float)suma / (float)count;
}

// ====== Callbacks ======
void OnTxDone(void){
  if (lastTx == TX_NIVEL) {
    Serial.println("✅ TX done (paquete NIVEL → Junta).");
  } else if (lastTx == TX_CAUDAL) {
    Serial.println("✅ TX done (paquete CAUDAL → Junta).");
  } else {
    Serial.println("✅ TX done (desconocido).");
  }
  lastTx = TX_NONE;
  lora_idle = true;
}
void OnTxTimeout(void){ Radio.Sleep(); Serial.println("⚠️ TX timeout."); lora_idle = true; }

// Recibir paquetes del NodoCaudal (formato: "CAU:<int>")
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  Radio.Sleep();
  uint16_t len = (size < (uint16_t)(BUFFER_SIZE - 1)) ? size : (BUFFER_SIZE - 1);
  memcpy(rxpacket, payload, len);
  rxpacket[len] = '\0';

  int val;
  if (sscanf((char*)rxpacket, "CAU:%d", &val) == 1) {
    latest_caudal_lpm = val;
    have_caudal = true;
    Serial.printf("📥 NodoCaudal → Reservorio | CAUDAL recibido: %d L/min (RSSI=%d, SNR=%d)\n",
                  latest_caudal_lpm, (int)rssi, (int)snr);
  } else {
    // Filtramos paquetes numéricos (podría escucharse el propio NIVEL)
    if (rxpacket[0] == '-' || (rxpacket[0] >= '0' && rxpacket[0] <= '9')) {
      // Ignorar silenciosamente
    } else {
      Serial.printf("ℹ️  RX no reconocido en reservorio: \"%s\" (RSSI=%d, SNR=%d)\n", rxpacket, (int)rssi, (int)snr);
    }
  }

  // Volver a escuchar
  lora_idle = true;
  // pequeña espera para no “oírse” a sí mismo cuando está muy cerca
  delay(80);
  lora_idle = false;
  Radio.Rx(0);
}

// ====== Setup ======
void setup() {
  Serial.begin(115200);
  Mcu.begin();

  display.init();
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  RadioEvents.RxDone    = OnRxDone;
  RadioEvents.TxDone    = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, TX_TIMEOUT_VALUE);

  // Arrancamos escuchando al NodoCaudal
  lora_idle = false;
  Radio.Rx(0);

  Serial.println("🚀 Transmisor del Reservorio listo: escucha CAUDAL y envía NIVEL+CAUDAL a la Junta.");
}

// ====== Loop ======
void loop() {
  // 1) Tomar 6 lecturas de NIVEL (IQR)
  for (int i = 0; i < NUM_LECTURAS; i++) {
    portValue           = analogRead(LEVEL_SENSOR);
    nivelVoltios        = (portValue * 3.3f) / 4095.0f;
    diferenciaVoltaje   = (nivelVoltios - valorsinsumergir);
    valorMaximoAbsoluto = 3.0f - valorsinsumergir;
    alturaActual        = ((diferenciaVoltaje * alturaMaxima) / valorMaximoAbsoluto) + 2;
    lecturas[i] = alturaActual;

    // OLED
    display.clear();
    display.drawString(0, 0,  "SINCHAL POZO");
    display.drawString(0, 20, String("Voltaje: ") + String(nivelVoltios, 2) + " V");
    display.drawString(0, 30, String("Altura: ")  + String(alturaActual) + " cm");
    display.display();

    // LOG claro de la lectura
    Serial.printf("🔎 Lectura %d/6 | ADC=%d  V=%.2f  Altura=%d cm\n",
                  i+1, portValue, nivelVoltios, alturaActual);

    // Dar tiempo al IRQ para recibir CAU mientras muestreamos
    for (int k = 0; k < 100; k++) { Radio.IrqProcess(); delay(10); } // ~1s atención RX
    if (lora_idle) { lora_idle = false; Radio.Rx(0); }               // volver a escuchar

    delay(9000); // completa ~10s por muestra
  }

  float mediaFiltrada = calcularIQRMedia(lecturas, NUM_LECTURAS);
  int   nivel_cm      = (int)mediaFiltrada;

  // LOG resumen del nivel calculado
  Serial.printf("✅ NIVEL (IQR) listo para enviar: %d cm\n", nivel_cm);

  // 2) ENVÍO #1 (LEGADO): SOLO EL ENTERO DEL NIVEL
  snprintf(txpacket, BUFFER_SIZE, "%d", nivel_cm);
  Serial.printf("📤 Enviando a Junta (NIVEL): \"%s\" (len %d)\n", txpacket, strlen(txpacket));
  lastTx = TX_NIVEL;
  Radio.Send((uint8_t*)txpacket, strlen(txpacket));
  while(!lora_idle){ Radio.IrqProcess(); }

  delay(150); // separación pequeña

  // 3) ENVÍO #2 (CAUDAL): si no llegó nada aún, envía 0 para que se vea en pruebas
  int caudal_lpm = have_caudal ? latest_caudal_lpm : 0;
  snprintf(txpacket, BUFFER_SIZE, "CAU:%d", caudal_lpm);
  Serial.printf("📤 Enviando a Junta (CAUDAL): \"%s\" (len %d)\n", txpacket, strlen(txpacket));
  lastTx = TX_CAUDAL;
  Radio.Send((uint8_t*)txpacket, strlen(txpacket));
  while(!lora_idle){ Radio.IrqProcess(); }

  // Regresar a RX de inmediato para seguir recibiendo CAU
  delay(80);
  lora_idle = false;
  Radio.Rx(0);

  // Tu ritmo ≈ 60 s (6 lecturas * 10s)
}
// ===================== FIN =====================
