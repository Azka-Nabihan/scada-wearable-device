#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>

// ============================================================================
// SCaDA Firmware: Multi-Sensor Fusion (MAX30102 + MCP9808 + IMU/Gyro) + LoRa TX
// Board: ESP32 DevKit V1 (30-Pin)
//
// Sensor & Modul yang Digunakan:
// 1. MAX30102       -> I2C (0x57) : Denyut Jantung (BPM) & SpO2
// 2. MCP9808        -> I2C (Auto-scan 0x18 - 0x1F / 0x48) : Suhu Kulit (°C)
// 3. IMU (BMI160/LSM6DS3) -> I2C (Auto-scan 0x68, 0x69, 0x6A, 0x6B) : Gyro & Accel
// 4. LoRa Ra-02     -> SPI (433 MHz, SX1278) : Pemancar Telemetri ke Posko
// ============================================================================

// --- Konfigurasi Pin I2C & SPI ---
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22

#define LORA_SCK            18
#define LORA_MISO           19
#define LORA_MOSI           23
#define LORA_SS             5
#define LORA_RST            14
#define LORA_DIO0           2

// --- Konfigurasi Buzzer & Tombol SOS (Tanpa LED) ---
#define PIN_BUZZER_PWM      25   // PWM Piezo Buzzer
#define PIN_SOS_BUTTON      27   // Tombol Push-Button (INPUT_PULLUP)

#define BUZZER_FREQ_HZ      2700 // Frekuensi resonansi piezo ~2.7 kHz
#define BUZZER_RES_BITS     8    // Resolusi 8-bit (0-255)

// --- Konfigurasi Frekuensi & Parameter LoRa ---
const long activeFrequency = 433000000L; // 433 MHz (Ra-02 SX1278 410-525 MHz)
#define ENABLE_LORA_CRC     true         // Ubah ke false jika receiver tanpa CRC

// --- Alamat I2C & Status Hardware ---
#define ADDR_MAX30102       0x57

uint8_t mcpAddr = 0;                     // Alamat I2C sensor suhu aktif
uint8_t imuAddr = 0;                     // Alamat I2C IMU aktif
uint8_t imu_type = 0;                    // 1 = BMI160, 2 = LSM6DS3

bool max30102_ok = false;
bool mcp9808_ok  = false;
bool imu_ok      = false;
bool lora_ok     = false;

// --- 1. Variabel MAX30102 & PI Gatekeeper ---
float bpm_val = 0.0;
float spo2_val = 0.0;
bool finger_detected = false;
float pi_val = 0.0;                       // Perfusion Index (%)
const float MIN_PERFUSION_INDEX = 0.4f;   // Ambang batas sinyal valid (0.4% per Proposal 2.3)
bool signal_quality_ok = false;

float dc_ir = 0, dc_red = 0;
float prev_ac_ir = 0;
float max_ac_ir = -99999, min_ac_ir = 99999;
float max_ac_red = -99999, min_ac_red = 99999;
bool peak_registered = false;
unsigned long last_beat_time = 0;

const byte RATE_SIZE = 12;
float rates[RATE_SIZE] = {0};
byte rate_spot = 0;
byte rate_count = 0;

// --- 2. Variabel Sensor Suhu ---
float skin_temp = 0.0;

// --- 3. Variabel IMU (Gyroscope & Accelerometer) & PASS FSM ---
float gx = 0.0, gy = 0.0, gz = 0.0; // Gyroscope (derajat per detik / dps)
float ax = 0.0, ay = 0.0, az = 0.0; // Accelerometer (g)
float gyro_rate = 0.0;              // Total kecepatan rotasi sudut (magnitude gyro)
float accel_magnitude = 1.0;
unsigned long last_motion_time = 0;
bool is_still = false;              // Status henti gerak / diam

// PASS FSM 3-Tahap (NFPA 1982)
enum AlarmState {
  ALARM_NORMAL = 0,    // State 0: Bergerak aktif / Aman
  ALARM_PRE_ALARM,     // State 1: Diam >= 15 detik (Beep lembut)
  ALARM_FULL_DISTRESS  // State 2: Diam >= 30 detik / SOS Manual / Vital Kritis
};
AlarmState current_alarm_state = ALARM_NORMAL;
bool sos_manual_triggered = false;

const unsigned long PRE_ALARM_TIMEOUT_MS  = 15000; // 15 detik diam
const unsigned long FULL_ALARM_TIMEOUT_MS = 30000; // 30 detik diam

// Non-blocking Buzzer Timing
unsigned long last_buzzer_toggle = 0;
bool buzzer_pwm_active = false;

// Debounce Tombol SOS
unsigned long last_btn_press_time = 0;
bool last_btn_state = HIGH;

// --- 4. Variabel mPSI (Modified Physiological Strain Index - Moran et al., 1999) ---
const float HR_BASELINE      = 75.0f;   // HR awal istirahat siaga (bpm)
const float HR_MAX           = 190.0f;  // Batas denyut kerja tinggi (bpm)
const float T_SKIN_BASELINE  = 33.0f;   // Batas bawah suhu kulit normal (°C)
const float T_SKIN_MAX       = 38.5f;   // Batas atas suhu kulit ekstrem (°C)

float mpsi_score = 0.0;
String mpsi_status = "SAFE";          // SAFE, WARN, atau CRIT

// --- 5. Variabel Transmisi LoRa & Timing ---
int packetCounter = 0;
unsigned long last_lora_tx_time = 0;
unsigned long last_imu_temp_time = 0;
unsigned long last_reconnect_time = 0;

// ============================================================================
// HELPER I2C
// ============================================================================
void writeI2C(uint8_t devAddr, uint8_t regAddr, uint8_t val) {
  Wire.beginTransmission(devAddr);
  Wire.write(regAddr);
  Wire.write(val);
  Wire.endTransmission();
}

// ============================================================================
// INISIALISASI & AUTO-SCAN SENSOR
// ============================================================================

void initMAX30102() {
  Wire.beginTransmission(ADDR_MAX30102);
  if (Wire.endTransmission() != 0) {
    max30102_ok = false;
    Serial.println("[MAX30102] TIDAK TERDETEKSI di 0x57. Cek pin SDA D21, SCL D22, 3V3.");
    return;
  }

  writeI2C(ADDR_MAX30102, 0x09, 0x40); // Reset chip
  delay(50);
  writeI2C(ADDR_MAX30102, 0x04, 0x00); // FIFO Write Pointer = 0
  writeI2C(ADDR_MAX30102, 0x05, 0x00); // Overflow Counter = 0
  writeI2C(ADDR_MAX30102, 0x06, 0x00); // FIFO Read Pointer = 0
  writeI2C(ADDR_MAX30102, 0x08, 0x4F); // Sample averaging 4
  writeI2C(ADDR_MAX30102, 0x09, 0x03); // SpO2 mode (Red + IR)
  writeI2C(ADDR_MAX30102, 0x0A, 0x27); // 4096 ADC, 100 Hz, 411us pulse width
  writeI2C(ADDR_MAX30102, 0x0C, 0x24); // Red LED ~7.2 mA
  writeI2C(ADDR_MAX30102, 0x0D, 0x24); // IR LED ~7.2 mA

  max30102_ok = true;
  Serial.println("[MAX30102] Siap di 0x57.");
}

bool scanAndInitMCP9808() {
  // Pindai rentang alamat standar MCP9808 (0x18 s.d 0x1F)
  for (uint8_t addr = 0x18; addr <= 0x1F; addr++) {
    Wire.beginTransmission(addr);
    Wire.write(0x06); // Register Manufacturer ID
    if (Wire.endTransmission() == 0) {
      Wire.requestFrom(addr, (uint8_t)2);
      if (Wire.available() >= 2) {
        uint16_t mfgID = (Wire.read() << 8) | Wire.read();
        if (mfgID == 0x0054) {
          mcpAddr = addr;
          mcp9808_ok = true;
          Serial.printf("[MCP9808] TERDETEKSI & Siap di alamat 0x%02X!\n", addr);
          return true;
        }
      }
    }
  }

  // Fallback: Jika chip adalah MAX30205 (alamat 0x48)
  Wire.beginTransmission(0x48);
  if (Wire.endTransmission() == 0) {
    mcpAddr = 0x48;
    mcp9808_ok = true;
    Serial.println("[SUHU] Terdeteksi sensor MAX30205 di alamat 0x48!");
    return true;
  }

  mcp9808_ok = false;
  return false;
}

bool scanAndInitIMU() {
  // 1. Pindai BMI160 (Alamat default 0x68 atau alternatif 0x69 jika SDO=3V3)
  for (uint8_t addr = 0x68; addr <= 0x69; addr++) {
    Wire.beginTransmission(addr);
    Wire.write(0x00); // Register CHIP_ID
    if (Wire.endTransmission() == 0) {
      Wire.requestFrom(addr, (uint8_t)1);
      uint8_t id = Wire.available() ? Wire.read() : 0;
      if (id == 0xD1) {
        imuAddr = addr;
        imu_type = 1;
        writeI2C(addr, 0x7E, 0x11); // Accel Normal Mode
        delay(50);
        writeI2C(addr, 0x7E, 0x15); // Gyro Normal Mode
        delay(50);
        writeI2C(addr, 0x41, 0x03); // Range Accel ±2g
        writeI2C(addr, 0x43, 0x00); // Range Gyro ±2000 dps
        imu_ok = true;
        last_motion_time = millis();
        Serial.printf("[IMU] BMI160 TERDETEKSI & Siap di alamat 0x%02X!\n", addr);
        return true;
      }
    }
  }

  // 2. Pindai LSM6DS3 (Alamat default 0x6A atau alternatif 0x6B jika SA0=3V3)
  for (uint8_t addr = 0x6A; addr <= 0x6B; addr++) {
    Wire.beginTransmission(addr);
    Wire.write(0x0F); // Register WHO_AM_I
    if (Wire.endTransmission() == 0) {
      Wire.requestFrom(addr, (uint8_t)1);
      uint8_t id = Wire.available() ? Wire.read() : 0;
      if (id == 0x69 || id == 0x6A || id == 0x6C) {
        imuAddr = addr;
        imu_type = 2;
        writeI2C(addr, 0x10, 0x40); // Accel 104 Hz ODR, ±2g
        writeI2C(addr, 0x11, 0x4C); // Gyro 104 Hz ODR, 2000 dps
        imu_ok = true;
        last_motion_time = millis();
        Serial.printf("[IMU] LSM6DS3 TERDETEKSI & Siap di alamat 0x%02X (ID: 0x%02X)!\n", addr, id);
        return true;
      }
    }
  }

  imu_ok = false;
  return false;
}

void initLoRaRadio() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setSPI(SPI);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // Validasi silikon register 0x42
  pinMode(LORA_SS, OUTPUT);
  digitalWrite(LORA_SS, LOW);
  SPI.transfer(0x42 & 0x7F);
  uint8_t chipVersion = SPI.transfer(0x00);
  digitalWrite(LORA_SS, HIGH);

  Serial.printf("[SPI CHECK] Silicon Version Register (0x42): 0x%02X\n", chipVersion);
  if (chipVersion != 0x12) {
    Serial.println("\n[ERROR] Koneksi SPI ke modul LoRa Ra-02 GAGAL!");
    Serial.printf("        Nilai terbaca: 0x%02X (Ekspektasi: 0x12)\n", chipVersion);
    lora_ok = false;
    return;
  }
  Serial.println("[SPI CHECK] Modul SX1276/SX1278 normal (0x12). Inisialisasi RF...");

  if (!LoRa.begin(activeFrequency)) {
    Serial.println("[GAGAL] Radio LoRa tidak dapat diaktifkan pada 433 MHz!");
    lora_ok = false;
    return;
  }

  LoRa.setTxPower(17);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);

  if (ENABLE_LORA_CRC) {
    LoRa.enableCrc();
  } else {
    LoRa.disableCrc();
  }

  lora_ok = true;
  Serial.printf("[BERHASIL] Pemancar LoRa SIAP pada frekuensi %.1f MHz!\n", activeFrequency / 1E6);
}

// ============================================================================
// METODE PEMBACAAN DATA SENSOR
// ============================================================================

void processMAX30102() {
  if (!max30102_ok) return;

  Wire.beginTransmission(ADDR_MAX30102);
  Wire.write(0x07);
  if (Wire.endTransmission(false) != 0) return;

  Wire.requestFrom((uint8_t)ADDR_MAX30102, (uint8_t)6);
  if (Wire.available() < 6) return;

  uint32_t red = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
  uint32_t ir  = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
  red &= 0x03FFFF;
  ir  &= 0x03FFFF;

  if (ir < 50000) {
    finger_detected = false;
    signal_quality_ok = false;
    pi_val = 0.0;
    bpm_val = 0.0;
    spo2_val = 0.0;
    dc_ir = 0;
    dc_red = 0;
    prev_ac_ir = 0;
    peak_registered = false;
    rate_count = 0;
    rate_spot = 0;
    return;
  }

  finger_detected = true;

  const float alpha = 0.95;
  if (dc_ir == 0) {
    dc_ir = ir;
    dc_red = red;
  } else {
    dc_ir = alpha * dc_ir + (1.0 - alpha) * ir;
    dc_red = alpha * dc_red + (1.0 - alpha) * red;
  }

  float ac_ir = ir - dc_ir;
  float ac_red = red - dc_red;

  if (ac_ir > max_ac_ir)   max_ac_ir = ac_ir;
  if (ac_ir < min_ac_ir)   min_ac_ir = ac_ir;
  if (ac_red > max_ac_red) max_ac_red = ac_red;
  if (ac_red < min_ac_red) min_ac_red = ac_red;

  if (prev_ac_ir > 85 && ac_ir < prev_ac_ir && !peak_registered) {
    unsigned long now = millis();
    if (last_beat_time == 0 || (now - last_beat_time) >= 2200) {
      last_beat_time = now;
    } else {
      unsigned long dt = now - last_beat_time;
      if (dt >= 650 && dt <= 1400) {
        float instant_bpm = 60000.0 / (float)dt;

        float vpp_ir = max_ac_ir - min_ac_ir;
        float vpp_red = max_ac_red - min_ac_red;

        // Hitung Perfusion Index (%)
        if (dc_ir > 1000) {
          pi_val = (vpp_ir / dc_ir) * 100.0f;
        } else {
          pi_val = 0.0f;
        }

        // --- GATEKEEPER: Validasi Kualitas Sinyal (Ambang Batas 0.4%) ---
        if (pi_val >= MIN_PERFUSION_INDEX && vpp_ir > 20 && dc_red > 1000) {
          signal_quality_ok = true;

          rates[rate_spot++] = instant_bpm;
          rate_spot %= RATE_SIZE;
          if (rate_count < RATE_SIZE) rate_count++;

          float sum = 0.0;
          for (byte i = 0; i < rate_count; i++) sum += rates[i];
          bpm_val = sum / (float)rate_count;

          last_beat_time = now;

          float r = (vpp_red / dc_red) / (vpp_ir / dc_ir);

          // Kalibrasi Linear Reflektif (Solusi 1):
          // Pada sensor pantulan (reflective PPG), rasio R berada di rentang ~0.75 - 1.05
          // Formula ini memetakan R=0.8 -> ~98%, R=0.9 -> ~97.3%, R=1.0 -> ~96.5%
          float calc_spo2 = 104.0 - 7.5 * r;

          if (calc_spo2 > 99.0) calc_spo2 = 99.0;
          if (calc_spo2 < 80.0) calc_spo2 = 80.0;

          if (spo2_val == 0) {
            spo2_val = calc_spo2;
          } else {
            // Filter perata eksponensial (85% stabilisator + 15% sampel baru)
            spo2_val = 0.85 * spo2_val + 0.15 * calc_spo2;
          }
        } else {
          // Derau gerakan / sinyal terlalu lemah: tolak sampel (data drop) & tahan nilai stabil
          signal_quality_ok = false;
        }

        max_ac_ir = -99999; min_ac_ir = 99999;
        max_ac_red = -99999; min_ac_red = 99999;
      }
    }
    peak_registered = true;
  } else if (ac_ir < 15) {
    peak_registered = false;
  }

  prev_ac_ir = ac_ir;
}

void readMCP9808() {
  if (!mcp9808_ok || mcpAddr == 0) return;

  if (mcpAddr == 0x48) {
    // MAX30205 format
    Wire.beginTransmission(0x48);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) { mcp9808_ok = false; return; }
    Wire.requestFrom((uint8_t)0x48, (uint8_t)2);
    if (Wire.available() >= 2) {
      int16_t raw = (Wire.read() << 8) | Wire.read();
      skin_temp = raw * 0.00390625f;
    }
  } else {
    // MCP9808 format
    Wire.beginTransmission(mcpAddr);
    Wire.write(0x05);
    if (Wire.endTransmission() != 0) {
      mcp9808_ok = false; // Sensor terputus
      return;
    }

    Wire.requestFrom(mcpAddr, (uint8_t)2);
    if (Wire.available() < 2) return;

    uint8_t upper = Wire.read() & 0x1F;
    uint8_t lower = Wire.read();

    if ((upper & 0x10) == 0x10) {
      upper &= 0x0F;
      skin_temp = 256.0f - ((upper * 16.0f) + (lower / 16.0f));
    } else {
      skin_temp = (upper * 16.0f) + (lower / 16.0f);
    }
  }
}

void readIMU() {
  if (!imu_ok || imuAddr == 0) return;

  int16_t raw_gx = 0, raw_gy = 0, raw_gz = 0;
  int16_t raw_ax = 0, raw_ay = 0, raw_az = 0;

  if (imu_type == 1) {
    // --- METODE BMI160 ---
    Wire.beginTransmission(imuAddr);
    Wire.write(0x0C);
    if (Wire.endTransmission(false) != 0) {
      imu_ok = false;
      return;
    }

    Wire.requestFrom(imuAddr, (uint8_t)12);
    if (Wire.available() >= 12) {
      raw_gx = Wire.read() | (Wire.read() << 8);
      raw_gy = Wire.read() | (Wire.read() << 8);
      raw_gz = Wire.read() | (Wire.read() << 8);
      raw_ax = Wire.read() | (Wire.read() << 8);
      raw_ay = Wire.read() | (Wire.read() << 8);
      raw_az = Wire.read() | (Wire.read() << 8);

      gx = raw_gx / 16.4f;
      gy = raw_gy / 16.4f;
      gz = raw_gz / 16.4f;
      ax = raw_ax / 16384.0f;
      ay = raw_ay / 16384.0f;
      az = raw_az / 16384.0f;
    }
  } else if (imu_type == 2) {
    // --- METODE LSM6DS3 ---
    Wire.beginTransmission(imuAddr);
    Wire.write(0x22);
    if (Wire.endTransmission(false) != 0) {
      imu_ok = false;
      return;
    }

    Wire.requestFrom(imuAddr, (uint8_t)12);
    if (Wire.available() >= 12) {
      raw_gx = Wire.read() | (Wire.read() << 8);
      raw_gy = Wire.read() | (Wire.read() << 8);
      raw_gz = Wire.read() | (Wire.read() << 8);
      raw_ax = Wire.read() | (Wire.read() << 8);
      raw_ay = Wire.read() | (Wire.read() << 8);
      raw_az = Wire.read() | (Wire.read() << 8);

      gx = raw_gx * 0.070f;
      gy = raw_gy * 0.070f;
      gz = raw_gz * 0.070f;
      ax = raw_ax * 0.000061f;
      ay = raw_ay * 0.000061f;
      az = raw_az * 0.000061f;
    }
  }

  gyro_rate = sqrt(gx * gx + gy * gy + gz * gz);
  accel_magnitude = sqrt(ax * ax + ay * ay + az * az);

  bool is_moving = (gyro_rate > 10.0f) || (fabs(accel_magnitude - 1.0f) > 0.15f);
  if (is_moving) {
    last_motion_time = millis();
    is_still = false;
    // Batal otomatis Pre-Alarm saat tangan bergerak (jika bukan dipicu manual SOS)
    if (current_alarm_state == ALARM_PRE_ALARM && !sos_manual_triggered) {
      current_alarm_state = ALARM_NORMAL;
    }
  } else {
    unsigned long still_duration = millis() - last_motion_time;
    if (still_duration >= FULL_ALARM_TIMEOUT_MS) {
      is_still = true;
      current_alarm_state = ALARM_FULL_DISTRESS;
    } else if (still_duration >= PRE_ALARM_TIMEOUT_MS) {
      is_still = true;
      if (current_alarm_state != ALARM_FULL_DISTRESS) {
        current_alarm_state = ALARM_PRE_ALARM;
      }
    }
  }
}

// ============================================================================
// LOGIKA TOMBOL DARURAT SOS & GENERATOR BUZZER NON-BLOCKING
// ============================================================================

void setBuzzerDuty(uint32_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(PIN_BUZZER_PWM, duty);
#else
  ledcWrite(0, duty);
#endif
}

void checkSOSButton() {
  bool reading = digitalRead(PIN_SOS_BUTTON);
  if (reading == LOW && last_btn_state == HIGH && (millis() - last_btn_press_time > 300)) {
    last_btn_press_time = millis();
    if (current_alarm_state == ALARM_FULL_DISTRESS) {
      Serial.println("\n[ALARM] Tombol ditekan: RESET alarm manual kembali ke NORMAL.");
      current_alarm_state = ALARM_NORMAL;
      sos_manual_triggered = false;
      last_motion_time = millis();
      is_still = false;
      setBuzzerDuty(0);
      buzzer_pwm_active = false;
    } else {
      Serial.println("\n[ALARM] Tombol SOS DITEKAN! Memicu FULL DISTRESS seketika.");
      current_alarm_state = ALARM_FULL_DISTRESS;
      sos_manual_triggered = true;
    }
  }
  last_btn_state = reading;
}

void updateBuzzerPattern() {
  unsigned long now = millis();

  switch (current_alarm_state) {
    case ALARM_NORMAL:
      if (buzzer_pwm_active) {
        setBuzzerDuty(0);
        buzzer_pwm_active = false;
      }
      break;

    case ALARM_PRE_ALARM:
      // Pola: Beep 100 ms ON (~60 dB, duty 128), 900 ms OFF
      if (buzzer_pwm_active && (now - last_buzzer_toggle >= 100)) {
        setBuzzerDuty(0);
        buzzer_pwm_active = false;
        last_buzzer_toggle = now;
      } else if (!buzzer_pwm_active && (now - last_buzzer_toggle >= 900)) {
        setBuzzerDuty(128);
        buzzer_pwm_active = true;
        last_buzzer_toggle = now;
      }
      break;

    case ALARM_FULL_DISTRESS:
      // Pola: Sirene cepat 120 ms ON (~90 dB, duty 220), 80 ms OFF
      if (buzzer_pwm_active && (now - last_buzzer_toggle >= 120)) {
        setBuzzerDuty(0);
        buzzer_pwm_active = false;
        last_buzzer_toggle = now;
      } else if (!buzzer_pwm_active && (now - last_buzzer_toggle >= 80)) {
        setBuzzerDuty(220);
        buzzer_pwm_active = true;
        last_buzzer_toggle = now;
      }
      break;
  }
}

// ============================================================================
// KALKULASI MODIFIED PHYSIOLOGICAL STRAIN INDEX (mPSI) - MORAN ET AL. (1999)
// ============================================================================
void updateMPSI() {
  // 1. Amankan nilai HR (jika jari lepas, gunakan baseline agar tidak bernilai minus)
  float effectiveHR = finger_detected ? bpm_val : HR_BASELINE;
  if (effectiveHR < HR_BASELINE) effectiveHR = HR_BASELINE;
  if (effectiveHR > HR_MAX) effectiveHR = HR_MAX;

  // 2. Komponen Suhu Kulit Mentah MCP9808 (Murni tanpa offset buatan)
  float effectiveSkin = skin_temp;
  if (effectiveSkin < T_SKIN_BASELINE) effectiveSkin = T_SKIN_BASELINE;
  if (effectiveSkin > T_SKIN_MAX) effectiveSkin = T_SKIN_MAX;

  // 3. Formula Moran et al. (1999): 50% Beban Jantung + 50% Beban Termal Kulit
  float hrComp   = (effectiveHR - HR_BASELINE) / (HR_MAX - HR_BASELINE);
  float tempComp = (effectiveSkin - T_SKIN_BASELINE) / (T_SKIN_MAX - T_SKIN_BASELINE);
  mpsi_score = 5.0f * hrComp + 5.0f * tempComp;

  // Batasi skala 0.0 - 10.0
  if (mpsi_score < 0.0f) mpsi_score = 0.0f;
  if (mpsi_score > 10.0f) mpsi_score = 10.0f;

  // 4. Evaluasi Kategori Risiko & FSM Alarm
  if (current_alarm_state == ALARM_FULL_DISTRESS || sos_manual_triggered ||
      (finger_detected && spo2_val > 0 && spo2_val < 90) || mpsi_score >= 6.5f) {
    mpsi_status = "CRIT"; // 🔴 Merah: Henti gerak 30s, SOS manual, hipoksia parah, atau heat strain tinggi
    current_alarm_state = ALARM_FULL_DISTRESS;
  } else if (current_alarm_state == ALARM_PRE_ALARM || mpsi_score >= 3.0f ||
             (finger_detected && spo2_val > 0 && spo2_val < 94)) {
    mpsi_status = "WARN"; // 🟡 Kuning: Pre-alarm 15s atau kelelahan meningkat
  } else {
    mpsi_status = "SAFE"; // 🟢 Hijau: Aman
  }
}

void transmitLoRaPacket() {
  if (!lora_ok) return;

  // Hitung metrik mPSI terbaru sebelum memancarkan paket
  updateMPSI();

  packetCounter++;

  String motion_str = is_still ? "STILL" : "MOVE";
  String alarm_str = (current_alarm_state == ALARM_FULL_DISTRESS) ? "SOS" :
                     ((current_alarm_state == ALARM_PRE_ALARM) ? "PRE" : "OK");

  String payload = "SCaDA Cuff #1";
  payload += " | HR: " + String((int)bpm_val) + " BPM";
  payload += " | SpO2: " + String((int)spo2_val) + "%";
  payload += " | Temp: " + String(skin_temp, 1) + " C";
  payload += " | mPSI: " + String(mpsi_score, 1) + "(" + mpsi_status + ")";
  payload += " | Stat: " + motion_str;
  payload += " | Alarm: " + alarm_str;
  payload += " | Pkt: " + String(packetCounter);

  Serial.printf("[TX #%d] (%d byte) %s ... ", packetCounter, payload.length(), payload.c_str());

  LoRa.beginPacket();
  LoRa.print(payload);
  int success = LoRa.endPacket();

  if (success == 1) {
    Serial.println("BERHASIL! (Gelombang radio terpancar)");
  } else {
    Serial.println("GAGAL memancarkan paket!");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("[SCaDA] ESP32 Multi-Sensor (Gyro) & LoRa TX");
  Serial.println("==========================================");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);

  // 1. Inisialisasi MAX30102
  initMAX30102();

  // 2. Deteksi & Inisialisasi Sensor Suhu
  if (!scanAndInitMCP9808()) {
    Serial.println("[SUHU] BELUM TERDETEKSI di 0x18-0x1F atau 0x48. Cek kabel VCC/GND/I2C.");
  }

  // 3. Deteksi & Inisialisasi IMU (BMI160 / LSM6DS3)
  if (!scanAndInitIMU()) {
    Serial.println("[IMU] BELUM TERDETEKSI di 0x68-0x6B. Cek kabel VCC/GND/I2C.");
  }

  // 4. Inisialisasi LoRa TX
  initLoRaRadio();

  // 5. Inisialisasi Aktuator Buzzer & Tombol SOS (Kompatibel ESP32 Core 3.x & 2.x)
  pinMode(PIN_SOS_BUTTON, INPUT_PULLUP);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcAttach(PIN_BUZZER_PWM, BUZZER_FREQ_HZ, BUZZER_RES_BITS);
#else
  ledcSetup(0, BUZZER_FREQ_HZ, BUZZER_RES_BITS);
  ledcAttachPin(PIN_BUZZER_PWM, 0);
#endif
  setBuzzerDuty(0);
  Serial.println("[OK] Buzzer (GPIO 25) & Tombol SOS (GPIO 27) aktif.");

  Serial.println("------------------------------------------");
  Serial.println("Mulai transmisi telemetri berkala (2 detik)...\n");
}

void loop() {
  unsigned long now = millis();

  // Periksa tombol darurat SOS & perbarui pola buzzer secara non-blocking
  checkSOSButton();
  updateBuzzerPattern();

  // Auto-Scan & Reconnect berkala jika sensor belum terbaca / sempat lepas
  if (!mcp9808_ok || !imu_ok) {
    if (now - last_reconnect_time >= 3000) {
      last_reconnect_time = now;
      if (!mcp9808_ok) scanAndInitMCP9808();
      if (!imu_ok) scanAndInitIMU();
    }
  }

  // 1. Sampling denyut MAX30102 FIFO
  processMAX30102();

  // 2. Sampling Suhu & Gyro/Accel tiap 200 ms
  if (now - last_imu_temp_time >= 200) {
    last_imu_temp_time = now;
    readMCP9808();
    readIMU();
  }

  // 3. Transmisi LoRa tiap 2.0 detik
  if (now - last_lora_tx_time >= 2000) {
    last_lora_tx_time = now;
    transmitLoRaPacket();

    String alarm_str = (current_alarm_state == ALARM_FULL_DISTRESS) ? "SOS" :
                       ((current_alarm_state == ALARM_PRE_ALARM) ? "PRE" : "OK");

    Serial.printf("     -> [DETAIL] Alarm: %s | Jari: %s (PI: %4.2f%%) | Temp: %.1f C | mPSI: %.1f (%s) | Gyro: X:%+5.1f Y:%+5.1f Z:%+5.1f (Mag: %4.1f dps) | Accel: X:%+.2f Y:%+.2f Z:%+.2f\n",
                  alarm_str.c_str(),
                  finger_detected ? (signal_quality_ok ? "OK" : "POOR") : "LEPAS",
                  pi_val,
                  skin_temp,
                  mpsi_score, mpsi_status.c_str(),
                  gx, gy, gz, gyro_rate,
                  ax, ay, az);
  }

  delay(20);
}