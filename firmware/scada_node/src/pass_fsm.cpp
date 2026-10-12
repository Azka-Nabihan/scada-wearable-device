#include "config.h"

// ============================================================================
// SCaDA PASS Finite State Machine & 2-Stage Acoustic Alarm Implementation
// Sesuai Regulasi NFPA 1982 & Pengganti Vibration Motor
// ============================================================================

enum PassState : uint8_t {
  PASS_STATE_NORMAL = 0,
  PASS_STATE_PRE_ALARM = 1,
  PASS_STATE_FULL_DISTRESS = 2,
  PASS_STATE_MANUAL_SOS = 3
};

static PassState currentPassState = PASS_STATE_NORMAL;
static unsigned long noMotionStartTime = 0;
static bool isMotionDetected = true;
static unsigned long lastChirpTime = 0;
static bool chirpState = false;

void initPassHardware() {
  pinMode(PIN_BMI160_INT1, INPUT_PULLDOWN);
  pinMode(PIN_SOS_BUTTON, INPUT_PULLUP);
  pinMode(PIN_STROBE_LED, OUTPUT);
  digitalWrite(PIN_STROBE_LED, LOW);

  // Setup LEDC PWM untuk Buzzer Piezo
  ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_LEDC_FREQ, BUZZER_LEDC_RES);
  ledcAttachPin(PIN_BUZZER_PWM, BUZZER_LEDC_CHANNEL);
  ledcWrite(BUZZER_LEDC_CHANNEL, 0); // Mute awal
}

void setMotionDetected(bool motion) {
  isMotionDetected = motion;
  if (motion) {
    if (currentPassState == PASS_STATE_PRE_ALARM) {
      // Petugas menggerakkan badan saat Pre-Alarm -> batalkan alarm dan reset ke normal
      currentPassState = PASS_STATE_NORMAL;
      ledcWrite(BUZZER_LEDC_CHANNEL, 0);
      digitalWrite(PIN_STROBE_LED, LOW);
    }
    noMotionStartTime = millis();
  }
}

void triggerManualSOS() {
  currentPassState = PASS_STATE_MANUAL_SOS;
}

PassState updatePassFSM() {
  unsigned long now = millis();

  // 1. Cek Tombol Darurat Manual
  if (digitalRead(PIN_SOS_BUTTON) == LOW) {
    currentPassState = PASS_STATE_MANUAL_SOS;
  }

  // 2. Evaluasi State Berdasarkan Waktu Henti Gerak
  if (currentPassState != PASS_STATE_MANUAL_SOS) {
    if (!isMotionDetected) {
      unsigned long stillDuration = now - noMotionStartTime;
      if (stillDuration >= PASS_FULL_ALARM_MS) {
        currentPassState = PASS_STATE_FULL_DISTRESS;
      } else if (stillDuration >= PASS_PRE_ALARM_MS) {
        currentPassState = PASS_STATE_PRE_ALARM;
      } else {
        currentPassState = PASS_STATE_NORMAL;
      }
    }
  }

  // 3. Eksekusi Aktuator Suara & Visual Sesuai State
  switch (currentPassState) {
    case PASS_STATE_NORMAL:
      ledcWrite(BUZZER_LEDC_CHANNEL, 0); // Hening
      digitalWrite(PIN_STROBE_LED, LOW);
      break;

    case PASS_STATE_PRE_ALARM:
      // Nada Intermittent Chirp Lembut (~60 dB) tiap 1 detik
      if (now - lastChirpTime >= 500) {
        lastChirpTime = now;
        chirpState = !chirpState;
        if (chirpState) {
          ledcWriteTone(BUZZER_LEDC_CHANNEL, 2000); // 2 kHz tone
          ledcWrite(BUZZER_LEDC_CHANNEL, 64);      // Volume rendah (duty 25%)
        } else {
          ledcWrite(BUZZER_LEDC_CHANNEL, 0);       // Hening
        }
      }
      digitalWrite(PIN_STROBE_LED, LOW);
      break;

    case PASS_STATE_FULL_DISTRESS:
    case PASS_STATE_MANUAL_SOS:
      // Sirene Darurat Melengking Penuh 90 dB + Strobo Kedip
      ledcWriteTone(BUZZER_LEDC_CHANNEL, (now / 250) % 2 == 0 ? 2700 : 3500); // Nada bergantian
      ledcWrite(BUZZER_LEDC_CHANNEL, 255); // Volume maksimal 100%
      digitalWrite(PIN_STROBE_LED, (now / 100) % 2 == 0 ? HIGH : LOW); // Strobo 5 Hz
      break;
  }

  return currentPassState;
}
