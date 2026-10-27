#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
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

// Feed once per second, after voltage validation and lid gating. The median
// rejects brief reflections without averaging the old fill into a refill.
class HopperLevelFilter {
 public:
  // Returns true only when the caller should publish result. Invalid input
  // publishes unknown immediately and clears the entire window. The first
  // valid sample after settling publishes immediately; subsequent checks
  // happen every 30 seconds and publish only changed whole percentages.
  bool update(float level, uint32_t now_ms, float &result) {
    if (!std::isfinite(level) || level < 0.0f || level > 100.0f) {
      count_ = next_ = 0;
      if (has_output_ && !std::isfinite(output_)) return false;
      has_output_ = true;
      output_ = result = NAN;
      return true;
    }
    samples_[next_] = level;
    next_ = (next_ + 1) % samples_.size();
    if (count_ < samples_.size()) ++count_;
    if (count_ != 1 && uint32_t(now_ms - last_check_ms_) < 30000) return false;
    last_check_ms_ = now_ms;
    auto ordered = samples_;
    std::sort(ordered.begin(), ordered.begin() + count_);
    float median = ordered[count_ / 2];
    if (count_ % 2 == 0) median = (median + ordered[count_ / 2 - 1]) / 2.0f;
    const float rounded = std::round(median);
    if (has_output_ && rounded == output_) return false;
    has_output_ = true;
    output_ = result = rounded;
    return true;
  }

 private:
  std::array<float, 31> samples_{};
  std::size_t count_{0};
  std::size_t next_{0};
  uint32_t last_check_ms_{0};
  bool has_output_{false};
  float output_{NAN};
};

// One hopper on this controller; share its window between the ADC and lid
// callbacks. A function-local instance avoids ESPHome globals being declared
// before custom include files, and starts fresh after each ESP reboot.
inline HopperLevelFilter &hopper_level_filter() {
  static HopperLevelFilter filter;
  return filter;
}

// Rate-limit diagnostics without hiding unknown/recovery transitions. This
// runs after the level calculation so that its one-second sampling is intact.
class HopperDiagnosticFilter {
 public:
  bool update(float value, uint32_t now_ms, float &result) {
    const bool valid = std::isfinite(value);
    if (has_output_ && valid == was_valid_ &&
        (!valid || uint32_t(now_ms - last_publish_ms_) < 15000)) return false;
    has_output_ = true;
    was_valid_ = valid;
    last_publish_ms_ = now_ms;
    result = valid ? value : NAN;
    return true;
  }

 private:
  bool has_output_{false};
  bool was_valid_{false};
  uint32_t last_publish_ms_{0};
};

inline HopperDiagnosticFilter &hopper_voltage_filter() {
  static HopperDiagnosticFilter filter;
  return filter;
}

inline HopperDiagnosticFilter &hopper_distance_filter() {
  static HopperDiagnosticFilter filter;
  return filter;
}

// The lid package can invalidate co-installed level entities without taking
// a dependency on them. Level-only and lid-only packages remain independent.
using HopperInvalidationCallback = void (*)();
inline HopperInvalidationCallback &hopper_invalidation_callback() {
  static HopperInvalidationCallback callback = nullptr;
  return callback;
}

}  // namespace stove_reporting
