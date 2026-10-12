#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

// ============================================================================
// SCaDA Base Station / Posko USB LoRa Receiver Firmware
// ESP32 DevKit V1 -> USB Serial ke Dashboard Laptop (JSON Stream)
// ============================================================================

// Pinout SPI LoRa SX1276 (Sama persis dengan Node)
#define PIN_LORA_SCK          18
#define PIN_LORA_MISO         19
#define PIN_LORA_MOSI         23
#define PIN_LORA_SS           5
#define PIN_LORA_RST          14
#define PIN_LORA_DIO0         2

#define LORA_FREQUENCY_HZ     915E6
#define LORA_SPREADING_FACTOR 8
#define LORA_SIGNAL_BANDWIDTH 125E3
#define LORA_CODING_RATE      5
#define LORA_TX_POWER_DBM     17

#define TOTAL_SQUAD_NODES     5
#define POLLING_INTERVAL_MS   3000 // Giliran polling tiap 3 detik

#pragma pack(push, 1)
struct TelemetryPacket {
  uint8_t nodeId;       // 1 Byte
  uint8_t heartRate;    // 1 Byte
  uint8_t spO2;         // 1 Byte
  int8_t  temperature;  // 1 Byte
  uint8_t respRate;     // 1 Byte
  uint8_t passState;    // 1 Byte
  uint8_t riskLevel;    // 1 Byte
  uint8_t checksum;     // 1 Byte
};
#pragma pack(pop)

static uint8_t currentPollingId = 1;
static unsigned long lastPollTime = 0;

uint8_t verifyChecksum(const TelemetryPacket& packet) {
  const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&packet);
  uint8_t xorSum = 0;
  for (size_t i = 0; i < sizeof(TelemetryPacket) - 1; ++i) {
    xorSum ^= ptr[i];
  }
  return xorSum;
}

void sendPollingPing(uint8_t targetNodeId) {
  LoRa.beginPacket();
  LoRa.write(targetNodeId); // Target Node ID
  LoRa.write(0xAA);         // Command: POLL_TELEMETRY
  LoRa.endPacket();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_SS);
  LoRa.setPins(PIN_LORA_SS, PIN_LORA_RST, PIN_LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY_HZ)) {
    Serial.println("{\"status\":\"error\",\"msg\":\"LoRa Init Failed\"}");
    while (1) delay(1000);
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setTxPower(LORA_TX_POWER_DBM);

  Serial.println("{\"status\":\"ready\",\"role\":\"base_station\",\"freq\":\"915MHz\"}");
}

void loop() {
  unsigned long now = millis();

  // 1. Scheduler Polling Master Terjadwal
  if (now - lastPollTime >= POLLING_INTERVAL_MS) {
    lastPollTime = now;
    sendPollingPing(currentPollingId);

    // Rotasi ke ID berikutnya (1 -> 2 -> 3 -> 4 -> 5 -> 1)
    currentPollingId++;
    if (currentPollingId > TOTAL_SQUAD_NODES) {
      currentPollingId = 1;
    }
  }

  // 2. Menerima Paket Balasan atau Broadcast SOS Darurat
  int packetSize = LoRa.parsePacket();
  if (packetSize == sizeof(TelemetryPacket)) {
    TelemetryPacket packet;
    LoRa.readBytes(reinterpret_cast<uint8_t*>(&packet), sizeof(TelemetryPacket));

    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    // Verifikasi Integritas Data via Checksum
    if (verifyChecksum(packet) == packet.checksum) {
      // Cetak JSON terstruktur untuk dikonsumsi Dashboard Laptop
      Serial.print("{\"type\":\"telemetry\",");
      Serial.print("\"node_id\":");      Serial.print(packet.nodeId);
      Serial.print(",\"hr\":");          Serial.print(packet.heartRate);
      Serial.print(",\"spo2\":");        Serial.print(packet.spO2);
      Serial.print(",\"temp\":");        Serial.print(packet.temperature);
      Serial.print(",\"rr\":");          Serial.print(packet.respRate);
      Serial.print(",\"pass\":");        Serial.print(packet.passState);
      Serial.print(",\"risk\":");        Serial.print(packet.riskLevel);
      Serial.print(",\"rssi\":");        Serial.print(rssi);
      Serial.print(",\"snr\":");         Serial.print(snr, 1);
      Serial.println("}");
    }
  }

  delay(10);
}
