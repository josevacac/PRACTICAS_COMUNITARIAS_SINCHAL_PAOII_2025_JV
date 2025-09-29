// ===== NodoCaudal_Simulador (envía "CAU:NN") =====
#include "LoRaWan_APP.h"
#include "Arduino.h"

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
#define BUFFER_SIZE            16

static RadioEvents_t RadioEvents;
bool  lora_idle = true;
char  txpacket[BUFFER_SIZE];

void OnTxDone(void)    { Serial.println("TX done..."); lora_idle = true; }
void OnTxTimeout(void) { Radio.Sleep(); Serial.println("TX Timeout..."); lora_idle = true; }

void setup() {
  Serial.begin(115200);
  Mcu.begin();

  RadioEvents.TxDone    = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, TX_TIMEOUT_VALUE);

  Serial.println("NodoCaudal listo (envía CAU:NN).");
}

int genCaudal() {
  // Simula caudal 0..60 L/min (cámbialo por tu lectura real cuando conectes el sensor)
  static int v = 0; 
  v = (v + 7) % 61;
  return v;
}

void loop() {
  if (!lora_idle) { Radio.IrqProcess(); return; }

  int caudal = genCaudal();                     // <-- aquí pondrás tu lectura real
  snprintf(txpacket, BUFFER_SIZE, "CAU:%d", caudal);

  Serial.printf("Enviando caudal: \"%s\"\n", txpacket);
  Radio.Send((uint8_t*)txpacket, strlen(txpacket));
  lora_idle = false;

  // Deja 2–5 s entre envíos para no saturar el aire ni el TRANSMISOR
  for (int i=0; i<200; ++i) { Radio.IrqProcess(); delay(10); }
}
