#pragma once

#include <cmath>
#include <cstdint>
#include <string>

// Reporting only. These helpers never send Tuya commands.
namespace stove_reporting {

inline bool link_ready(bool initialized, uint32_t last_rx_ms, uint32_t now_ms) {
  return initialized && last_rx_ms != 0 && uint32_t(now_ms - last_rx_ms) < 45000;
}

inline bool confirmed_heating(bool heat_requested, bool connected, float state_code) {
  // DP112=12 observed with the panel in ECO2 and exhaust at 284 F.
  // Other burning/startup phases still need panel-correlated captures.
  return heat_requested && connected && state_code == 12.0f;
}

inline std::string operating_state(bool connected, float state_code) {
  if (!connected) return "Unavailable (MCU link)";
  if (!std::isfinite(state_code)) return "Unknown (awaiting status)";
  if (state_code == 10.0f) return "Standby";
  if (state_code == 12.0f) return "ECO2";
  if (state_code < 0 || state_code > 255 || std::floor(state_code) != state_code)
    return "Unknown (invalid status)";
  return "Unknown (code " + std::to_string(static_cast<int>(state_code)) + ")";
}

inline bool hopper_sample_ready(bool lid_known, bool lid_open, uint32_t changed_ms, uint32_t now_ms) {
  return lid_known && !lid_open && uint32_t(now_ms - changed_ms) >= 3000;
}

inline bool optional_hopper_sample_ready(bool enabled, bool known, bool open,
                                        uint32_t changed_ms, uint32_t now_ms) {
  return !enabled || hopper_sample_ready(known, open, changed_ms, now_ms);
}

// Provisional inverse-voltage model, NOT a calibrated Sharp transfer curve.
// A measured pellet reference is required; card readings are not equivalent.
inline float estimated_pellet_distance_cm(float volts, float reference_volts, float reference_cm) {
  if (!std::isfinite(volts) || volts <= 0 || !std::isfinite(reference_volts) || reference_volts <= 0 ||
      !std::isfinite(reference_cm) || reference_cm < 4 || reference_cm > 30) return NAN;
  const float distance = reference_cm * reference_volts / volts;
  // Guard against invalid values and extrapolation outside the rated range.
  // Too-close optical ambiguity cannot be detected reliably from voltage.
  if (!std::isfinite(distance) || distance < 4 || distance > 30) return NAN;
  return distance;
}

inline float estimated_pellet_level(float volts, float reference_volts, float reference_cm,
                                   float full_cm, float empty_cm) {
  const float distance = estimated_pellet_distance_cm(volts, reference_volts, reference_cm);
  if (!std::isfinite(distance) || !std::isfinite(full_cm) || !std::isfinite(empty_cm) ||
      full_cm < 0 || empty_cm <= full_cm) return NAN;
  const float level = 100.0f * (empty_cm - distance) / (empty_cm - full_cm);
  return level < 0 ? 0 : level > 100 ? 100 : level;
}

}  // namespace stove_reporting
