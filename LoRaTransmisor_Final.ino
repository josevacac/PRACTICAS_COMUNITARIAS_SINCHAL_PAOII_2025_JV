// Librerías necesarias
#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"

// OLED
#ifdef WIRELESS_STICK_V3
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);
#else
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
#endif

// Sensor
#define LEVEL_SENSOR 4

// LoRa config
#define RF_FREQUENCY           915000000
#define TX_OUTPUT_POWER        21
#define LORA_BANDWIDTH         0
#define LORA_SPREADING_FACTOR  11
#define LORA_CODINGRATE        1
#define LORA_PREAMBLE_LENGTH   8
#define LORA_SYMBOL_TIMEOUT    0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON   false
#define TX_TIMEOUT_VALUE       8000
#define RX_TIMEOUT_VALUE       3000
#define BUFFER_SIZE            10

// Calibración para sensor de nivel (Voltaje Multímetro + datos anteriores)
const float voltage0 = 0.594;   // 0 metros (multímetro)
const float level0 = 0.0;
const float voltage1 = 0.719;   // 0.2 metros (multímetro)
const float level1 = 20.0;
const float voltage2 = 0.828;   // 0.5 metros (datos anteriores)
const float level2 = 50.0;
const float voltage3 = 0.880;   // 0.7 metros (multímetro)
const float level3 = 70.0;
const float voltage4 = 0.923;   // 1.0 metros (datos anteriores)
const float level4 = 100.0;
const float voltage5 = 1.118;   // 1.2 metros (multímetro)
const float level5 = 120.0;
const float voltage6 = 1.357;   // 1.5 metros (datos anteriores)
const float level6 = 156.0;
const float voltage7 = 1.450;   // 1.7 metros (multímetro)
const float level7 = 170.0;
const float voltage8 = 1.538;   // 2.0 metros (datos anteriores)
const float level8 = 200.0;
const float voltage9 = 1.774;   // 2.5 metros (datos anteriores)
const float level9 = 250.0;
const float voltage10 = 2.009;  // 3.0 metros (datos anteriores)
const float level10 = 300.0;
const float voltage11 = 2.150;  // 3.3 metros (datos anteriores)
const float level11 = 330.0;
const float voltage12 = 2.241;  // 3.5 metros (datos anteriores)
const float level12 = 350.0;
const float voltage13 = 2.475;  // 4.0 metros (datos anteriores)
const float level13 = 400.0;
const float voltage14 = 2.617;  // 4.3 metros (datos anteriores)
const float level14 = 430.0;

// Variables físicas
int alturaMaxima = 500;
float valorsinsumergir = 0.60;
int portValue;
float nivelVoltios;
float diferenciaVoltaje;
float valorMaximoAbsoluto;
int alturaActual;

// IQR y Lecturas
#define NUM_LECTURAS 3
int lecturas[NUM_LECTURAS];

// LoRa
char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];
bool lora_idle = true;
static RadioEvents_t RadioEvents;

void OnTxDone(void);
void OnTxTimeout(void);

// === FUNCIONES AUXILIARES ===

// Ordenar un array (Bubble sort)
void ordenarArray(int *arr, int n) {
    for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (arr[j] > arr[j+1]) {
                int temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }
}

// Calcular media de valores dentro del IQR
float calcularIQRMedia(int *arr, int n) {
    ordenarArray(arr, n);
    float Q1 = (arr[1] + arr[2]) / 2.0;
    float Q3 = (arr[3] + arr[4]) / 2.0;
    float IQR = Q3 - Q1;
    float LI = Q1 - 1.5 * IQR;
    float LS = Q3 + 1.5 * IQR;

    int suma = 0;
    int count = 0;

    for (int i = 0; i < n; i++) {
        if (arr[i] >= LI && arr[i] <= LS) {
            suma += arr[i];
            count++;
        }
    }

    if (count == 0) return -1; // evitar división por cero
    return ((float)suma / count);
}


float calculateLevel(float measuredVoltage) {
  float level;
  
  if (measuredVoltage <= voltage0) {
    level = level0;
  } 
  else if (measuredVoltage < voltage1) {
    float slope = (level1 - level0) / (voltage1 - voltage0);
    level = level0 + (measuredVoltage - voltage0) * slope;
  } 
  else if (measuredVoltage < voltage2) {
    float slope = (level2 - level1) / (voltage2 - voltage1);
    level = level1 + (measuredVoltage - voltage1) * slope;
  } 
  else if (measuredVoltage < voltage3) {
    float slope = (level3 - level2) / (voltage3 - voltage2);
    level = level2 + (measuredVoltage - voltage2) * slope;
  } 
  else if (measuredVoltage < voltage4) {
    float slope = (level4 - level3) / (voltage4 - voltage3);
    level = level3 + (measuredVoltage - voltage3) * slope;
  } 
  else if (measuredVoltage < voltage5) {
    float slope = (level5 - level4) / (voltage5 - voltage4);
    level = level4 + (measuredVoltage - voltage4) * slope;
  } 
  else if (measuredVoltage < voltage6) {
    float slope = (level6 - level5) / (voltage6 - voltage5);
    level = level5 + (measuredVoltage - voltage5) * slope;
  } 
  else if (measuredVoltage < voltage7) {
    float slope = (level7 - level6) / (voltage7 - voltage6);
    level = level6 + (measuredVoltage - voltage6) * slope;
  } 
  else if (measuredVoltage < voltage8) {
    float slope = (level8 - level7) / (voltage8 - voltage7);
    level = level7 + (measuredVoltage - voltage7) * slope;
  } 
  else if (measuredVoltage < voltage9) {
    float slope = (level9 - level8) / (voltage9 - voltage8);
    level = level8 + (measuredVoltage - voltage8) * slope;
  } 
  else if (measuredVoltage < voltage10) {
    float slope = (level10 - level9) / (voltage10 - voltage9);
    level = level9 + (measuredVoltage - voltage9) * slope;
  } 
  else if (measuredVoltage < voltage11) {
    float slope = (level11 - level10) / (voltage11 - voltage10);
    level = level10 + (measuredVoltage - voltage10) * slope;
  } 
  else if (measuredVoltage < voltage12) {
    float slope = (level12 - level11) / (voltage12 - voltage11);
    level = level11 + (measuredVoltage - voltage11) * slope;
  } 
  else if (measuredVoltage < voltage13) {
    float slope = (level13 - level12) / (voltage13 - voltage12);
    level = level12 + (measuredVoltage - voltage12) * slope;
  } 
  else if (measuredVoltage <= voltage14) {
    float slope = (level14 - level13) / (voltage14 - voltage13);
    level = level13 + (measuredVoltage - voltage13) * slope;
  } 
  else {
    float slope = (level14 - level13) / (voltage14 - voltage13);
    level = level14 + (measuredVoltage - voltage14) * slope;
  }
  
  return level;
}

// === SETUP ===
void setup() {
    Serial.begin(115200);
    Mcu.begin();

    display.init();
    display.clear();
    display.display();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);

    // LoRa
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                      true, 0, 0, LORA_IQ_INVERSION_ON, TX_TIMEOUT_VALUE);
}

// === LOOP PRINCIPAL ===
void loop() {
    if (lora_idle) {
        for (int i = 0; i < NUM_LECTURAS; i++) {
            portValue = analogRead(LEVEL_SENSOR);
            nivelVoltios = (portValue * 3.3) / 4095.0;
            //Serial.print(nivelVoltios);


            //diferenciaVoltaje = (nivelVoltios - valorsinsumergir);
            //valorMaximoAbsoluto = 3.0 - valorsinsumergir;
            //alturaActual = ((diferenciaVoltaje * alturaMaxima) / valorMaximoAbsoluto) + 2;
	          float alturaActual = calculateLevel(nivelVoltios);
            //Serial.print(alturaActual);

            lecturas[i] = alturaActual;

            // Mostrar en OLED
            display.clear();
            display.drawString(0, 0, "SINCHAL POZO");
            display.drawString(0, 20, "Voltaje: " + String(nivelVoltios, 2) + " V");
            display.drawString(0, 30, "Altura: " + String(alturaActual) + " cm");
            display.display();

            delay(10000); // 10 segundos entre lecturas
        }

        float mediaFiltrada = calcularIQRMedia(lecturas, NUM_LECTURAS);
        int mediaTruncada = int(mediaFiltrada);

        display.clear();
        display.drawString(0, 0, "SINCHAL POZO");
        display.drawString(0, 20, "Altura Promedio: "+ String(mediaTruncada) + " cm");
        display.display();

        sprintf(txpacket, "%d", mediaTruncada);
        Serial.printf("\r\nEnviando: \"%s\", length %d\n", txpacket, strlen(txpacket));
        Radio.Send((uint8_t *)txpacket, strlen(txpacket));
        lora_idle = false;
        delay(5000);
    }

    Radio.IrqProcess();
}

void OnTxDone(void) {
    Serial.println("TX done...");
    lora_idle = true;
}

void OnTxTimeout(void) {
    Radio.Sleep();
    Serial.println("TX Timeout...");
    lora_idle = true;
}



if (sscanf(rxpacket, "%d", &valorRecibido) == 1) {
    enviarDatos(valorRecibido); // Field1
}
if (strncmp(rxpacket, "CAU:", 4) == 0) {
    int caudal;
    sscanf(rxpacket + 4, "%d", &caudal);
    enviarDatos2(caudal); // Field2
}





