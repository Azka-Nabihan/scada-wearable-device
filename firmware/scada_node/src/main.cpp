#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include "config.h"

// Forward Declarations dari modul pendukung
void initPassHardware();
void setMotionDetected(bool motion);
uint8_t updatePassFSM();
float calculatePSI(float currentHR, float currentSkinTemp);
uint8_t evaluateRiskLevel(float psiScore, uint8_t spO2, uint8_t passState);

// ============================================================================
// FreeRTOS Queues & Global State
// ============================================================================
static QueueHandle_t telemetryQueue;
static volatile bool g_bmi160InterruptFlag = false;

// ISR Handler untuk Pin Interrupt Hardware BMI160
void IRAM_ATTR onBmi160Interrupt() {
  g_bmi160InterruptFlag = true;
}

// Checksum XOR 8-bit
uint8_t calculateChecksum(const TelemetryPacket& packet) {
  const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&packet);
  uint8_t xorSum = 0;
  for (size_t i = 0; i < sizeof(TelemetryPacket) - 1; ++i) {
    xorSum ^= ptr[i];
  }
  return xorSum;
}

// ============================================================================
// CORE 1: Sensor Sampling, DSP Filtering, PASS FSM & PSI Calculation Task
// ============================================================================
void vSensorTask(void* pvParameters) {
  TelemetryPacket packet;
  packet.nodeId = DEFAULT_NODE_ID;

  // Variabel simulasi & smoothing biosinyal
  float simulatedHR = 82.0f;
  float simulatedTemp = 36.6f;
  uint8_t simulatedSpO2 = 98;
  uint8_t simulatedRR = 16;

  for (;;) {
    // 1. Cek Sinyal Interupsi Hardware BMI160
    if (g_bmi160InterruptFlag) {
      g_bmi160InterruptFlag = false;
      setMotionDetected(false); // Terdeteksi henti gerak oleh hardware
    } else {
      setMotionDetected(true);
    }

    // 2. Update Finite State Machine PASS (Akustik 2-Stage)
    uint8_t passState = updatePassFSM();

    // 3. Kalkulasi Skor Fisiologis (Moran PSI) & Evaluasi Risiko
    float psi = calculatePSI(simulatedHR, simulatedTemp);
    uint8_t risk = evaluateRiskLevel(psi, simulatedSpO2, passState);

    // 4. Susun Paket Telemetri 8-Byte
    packet.heartRate   = static_cast<uint8_t>(simulatedHR);
    packet.spO2        = simulatedSpO2;
    packet.temperature = static_cast<int8_t>(simulatedTemp);
    packet.respRate    = simulatedRR;
    packet.passState   = passState;
    packet.riskLevel   = risk;
    packet.checksum    = calculateChecksum(packet);

    // 5. Dorong ke FreeRTOS Queue (Non-Blocking Overwrite)
    xQueueOverwrite(telemetryQueue, &packet);

    vTaskDelay(pdMS_TO_TICKS(100)); // Siklus refresh 100 ms (10 Hz)
  }
}

// ============================================================================
// CORE 0: LoRa Direct Transceiver & Priority Preemption Task
// ============================================================================
void vLoRaTask(void* pvParameters) {
  TelemetryPacket currentPacket;

  for (;;) {
    // 1. Cek Paket Masuk dari Base Station (Polling PING)
    int packetSize = LoRa.parsePacket();
    if (packetSize > 0) {
      uint8_t targetId = LoRa.read();
      uint8_t cmdCode  = LoRa.read();

      // Jika Posko memanggil ID kita (Polling match)
      if (targetId == DEFAULT_NODE_ID && cmdCode == 0xAA) {
        if (xQueuePeek(telemetryQueue, &currentPacket, 0) == pdTRUE) {
          LoRa.beginPacket();
          LoRa.write(reinterpret_cast<uint8_t*>(&currentPacket), sizeof(TelemetryPacket));
          LoRa.endPacket();
        }
      }
    }

    // 2. Mode Priority Preemption: Jika PASS Darurat / SOS Aktif, Kirim Instan!
    if (xQueuePeek(telemetryQueue, &currentPacket, 0) == pdTRUE) {
      if (currentPacket.passState == 2 || currentPacket.passState == 3) {
        // Broadcast SOS darurat tanpa antre
        LoRa.beginPacket();
        LoRa.write(reinterpret_cast<uint8_t*>(&currentPacket), sizeof(TelemetryPacket));
        LoRa.endPacket();
        vTaskDelay(pdMS_TO_TICKS(500)); // Ulangi kirim tiap 500 ms saat darurat
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Yielding CPU Core 0
  }
}

// ============================================================================
// Setup & Initialization
// ============================================================================
void setup() {
  Serial.begin(115200);

  // 1. Matikan Total Radio Wi-Fi & Bluetooth untuk Menghemat Baterai
  WiFi.mode(WIFI_OFF);
  btStop();

  // 2. Inisialisasi Bus I2C
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);

  // 3. Inisialisasi Hardware PASS & Buzzer
  initPassHardware();
  attachInterrupt(digitalPinToInterrupt(PIN_BMI160_INT1), onBmi160Interrupt, RISING);

  // 4. Inisialisasi Modul LoRa Ra-02 (SX1276)
  SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_SS);
  LoRa.setPins(PIN_LORA_SS, PIN_LORA_RST, PIN_LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY_HZ)) {
    Serial.println("[ERROR] Inisialisasi LoRa gagal! Periksa kabel SPI.");
    while (1) delay(1000);
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setTxPower(LORA_TX_POWER_DBM);

  Serial.println("[SCaDA] Node Wearable Siap (Direct LoRa 915 MHz Active).");

  // 5. Inisialisasi FreeRTOS Queue (Kapasitas 1 Item / Overwrite)
  telemetryQueue = xQueueCreate(1, sizeof(TelemetryPacket));

  // 6. Spawn Asymmetric Dual-Core Tasks
  xTaskCreatePinnedToCore(
    vLoRaTask,
    "LoRaTask",
    4096,
    NULL,
    2, // Priority 2 (High)
    NULL,
    0  // CORE 0 (PRO_CPU)
  );

  xTaskCreatePinnedToCore(
    vSensorTask,
    "SensorTask",
    8192,
    NULL,
    1, // Priority 1
    NULL,
    1  // CORE 1 (APP_CPU)
  );
}

void loop() {
  // FreeRTOS vTaskDelete murni mengosongkan superloop default
  vTaskDelete(NULL);
}
