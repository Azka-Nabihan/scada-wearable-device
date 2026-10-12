#include <Arduino.h>

// ============================================================================
// Physiological Strain Index (PSI) Calculator
// Referensi: Moran et al. (1998) - American Physiological Society
// PSI = 5 * (HR_t - HR_0)/(HR_max - HR_0) + 5 * (T_core_t - T_core_0)/(39.5 - T_core_0)
// ============================================================================

// Nilai baseline rata-rata pemadam kebakaran saat siaga
const float HR_BASELINE   = 75.0f;   // HR_0 (bpm)
const float HR_MAX        = 190.0f;  // HR_max (bpm)
const float T_CORE_BASE   = 36.8f;   // T_0 (°C)
const float T_CORE_MAX    = 39.5f;   // T_max (°C)

enum RiskCategory : uint8_t {
  RISK_SAFE = 0,    // Hijau: PSI 0.0 - 2.9 (No / Low Strain)
  RISK_WARNING = 1, // Kuning: PSI 3.0 - 6.5 (Moderate Strain - Siapkan Rotasi)
  RISK_DANGER = 2   // Merah: PSI > 6.5 (High/Very High Strain - Evakuasi Darurat)
};

float calculatePSI(float currentHR, float currentSkinTemp) {
  // Koreksi suhu kulit ke perkiraan suhu inti (core temperature offset ~0.8°C di mikroklimat APD)
  float estimatedCoreTemp = currentSkinTemp + 0.8f;

  // Clamp input batas aman
  if (currentHR < HR_BASELINE) currentHR = HR_BASELINE;
  if (currentHR > HR_MAX) currentHR = HR_MAX;
  if (estimatedCoreTemp < T_CORE_BASE) estimatedCoreTemp = T_CORE_BASE;
  if (estimatedCoreTemp > T_CORE_MAX) estimatedCoreTemp = T_CORE_MAX;

  float hrComponent = (currentHR - HR_BASELINE) / (HR_MAX - HR_BASELINE);
  float tempComponent = (estimatedCoreTemp - T_CORE_BASE) / (T_CORE_MAX - T_CORE_BASE);

  float psi = 5.0f * hrComponent + 5.0f * tempComponent;

  // Batasi pada skala 0 - 10
  if (psi < 0.0f) psi = 0.0f;
  if (psi > 10.0f) psi = 10.0f;

  return psi;
}

RiskCategory evaluateRiskLevel(float psiScore, uint8_t spO2, uint8_t passState) {
  // Jika PASS alarm aktif (man-down) atau SpO2 drop < 85%, langsung kategorikan BAHAYA (Merah)
  if (passState == 2 || passState == 3 || spO2 < 85) {
    return RISK_DANGER;
  }

  if (psiScore >= 6.5f || spO2 < 90) {
    return RISK_DANGER;
  } else if (psiScore >= 3.0f || spO2 < 94) {
    return RISK_WARNING;
  } else {
    return RISK_SAFE;
  }
}
