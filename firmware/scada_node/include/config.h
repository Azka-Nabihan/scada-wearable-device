#pragma once
#include <Arduino.h>

// ============================================================================
// SCaDA Node Configuration & Pinout Definitions (ESP32 DevKit V1 30-Pin)
// ============================================================================

// 1. I2C Bus Configuration (Shared for MAX30102, MAX30205, BMI160)
#define PIN_I2C_SDA           21
#define PIN_I2C_SCL           22
#define I2C_FREQ_HZ           400000 // 400 kHz Fast-Mode I2C

// I2C Addresses
#define I2C_ADDR_BMI160       0x68
#define I2C_ADDR_MAX30102     0x57
#define I2C_ADDR_MAX30205     0x48

// 2. Hardware Interrupts & Inputs
#define PIN_BMI160_INT1       4   // Hardware No-Motion Interrupt from BMI160
#define PIN_SOS_BUTTON        27  // Waterproof Push Button (Pull-Up active)

// 3. Actuators & Outputs
#define PIN_BUZZER_PWM        25  // LEDC PWM channel for Pre-Alarm & Siren
#define PIN_STROBE_LED        26  // 5W Visual Distress Strobe LED (MOSFET gate)

#define BUZZER_LEDC_CHANNEL   0
#define BUZZER_LEDC_FREQ      2700 // Resonant freq piezo ~2.7 kHz
#define BUZZER_LEDC_RES       8    // 8-bit resolution (0-255)

// 4. LoRa SX1276 (Ra-02 915 MHz) SPI Configuration
#define PIN_LORA_SCK          18
#define PIN_LORA_MISO         19
#define PIN_LORA_MOSI         23
#define PIN_LORA_SS           5
#define PIN_LORA_RST          14
#define PIN_LORA_DIO0         2

#define LORA_FREQUENCY_HZ     915E6
#define LORA_SPREADING_FACTOR 8     // SF8
#define LORA_SIGNAL_BANDWIDTH 125E3 // 125 kHz
#define LORA_CODING_RATE      5     // 4/5
#define LORA_TX_POWER_DBM     17    // 17 dBm (~50 mW)

// 5. GPS NEO-6M Serial UART2
#define PIN_GPS_RX            16
#define PIN_GPS_TX            17
#define GPS_BAUD_RATE         9600

// 6. PASS (Personal Alert Safety System) Timing (NFPA 1982)
#define PASS_PRE_ALARM_MS     15000 // 15 seconds no motion
#define PASS_FULL_ALARM_MS    30000 // 30 seconds no motion

// 7. Node ID Configuration
#define DEFAULT_NODE_ID       1

// 8. Compact 8-Byte Telemetry Data Structure
#pragma pack(push, 1)
struct TelemetryPacket {
  uint8_t nodeId;       // 1 Byte: Unique Firefighter ID
  uint8_t heartRate;    // 1 Byte: Heart Rate in BPM (30 - 240)
  uint8_t spO2;         // 1 Byte: SpO2 in % (70 - 100)
  int8_t  temperature;  // 1 Byte: Skin Temp in Celsius (-10 to +80)
  uint8_t respRate;     // 1 Byte: Estimated Respiration Rate (0 - 60 BPM)
  uint8_t passState;    // 1 Byte: 0=Normal, 1=PreAlarm, 2=FullDistress, 3=ManualSOS
  uint8_t riskLevel;    // 1 Byte: 0=Safe (Green), 1=Warning (Yellow), 2=Danger (Red)
  uint8_t checksum;     // 1 Byte: Simple XOR checksum
};
#pragma pack(pop)
