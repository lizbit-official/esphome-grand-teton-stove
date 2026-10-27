#include "../includes/stove-reporting.h"
#include <cassert>
#include <iostream>
#include <limits>

using namespace stove_reporting;

int main() {
  // Regression: ECO2 still burns above setpoint; action must use observed
  // MCU status, not the room/target temperature difference.
  assert(confirmed_heating(true, true, 12));
  assert(!confirmed_heating(false, true, 12));
  assert(!confirmed_heating(true, false, 12));
  assert(!confirmed_heating(true, true, NAN));
  for (int code = 0; code <= 255; ++code)
    if (code != 12) assert(!confirmed_heating(true, true, code));
  assert(operating_state(true, 12) == "ECO2");
  assert(operating_state(true, 10) == "Standby");
  assert(operating_state(true, 7) == "Unknown (code 7)");
  assert(operating_state(true, NAN) == "Unknown (awaiting status)");
  assert(operating_state(true, 1.5f) == "Unknown (invalid status)");
  assert(operating_state(false, 12) == "Unavailable (MCU link)");

  assert(!link_ready(false, 10, 20));
  assert(!link_ready(true, 0, 20));
  assert(link_ready(true, 10, 45009));
  assert(!link_ready(true, 10, 45010));
  const uint32_t before_wrap = UINT32_MAX - 1000;
  assert(link_ready(true, before_wrap, 2000));
  assert(!link_ready(true, before_wrap, 45000));

  assert(!hopper_sample_ready(false, false, 100, 4000));
  assert(!hopper_sample_ready(true, true, 100, 4000));
  assert(!hopper_sample_ready(true, false, 100, 3099));
  assert(hopper_sample_ready(true, false, 100, 3100));
  assert(hopper_sample_ready(true, false, before_wrap, 2000));

  // Level-only works without a GPIO entity; adding the lid enables gating.
  assert(optional_hopper_sample_ready(false, false, true, 0, 1));
  assert(!optional_hopper_sample_ready(true, false, false, 0, 4000));
  assert(!optional_hopper_sample_ready(true, true, true, 0, 4000));
  assert(!optional_hopper_sample_ready(true, true, false, 1000, 3999));
  assert(optional_hopper_sample_ready(true, true, false, 1000, 4000));

  // Original installation's measured pellet reference remains reproducible.
  auto distance = [](float v) { return estimated_pellet_distance_cm(v, 0.444f, 23); };
  auto level = [](float v) { return estimated_pellet_level(v, 0.444f, 23, 3, 33); };
  assert(std::abs(distance(0.444f) - 23) < 0.001f);
  assert(std::abs(level(0.444f) - 33.3333f) < 0.001f);
  assert(level(0.430f) < level(0.444f));
  assert(level(0.630f) > level(0.444f));
  for (float bad : {NAN, 0.0f, -0.1f, 0.1f, 3.0f, std::numeric_limits<float>::infinity()})
    assert(std::isnan(level(bad)));
  for (float v = 0.341f; v < 2.55f; v += 0.01f)
    assert(std::isfinite(level(v)) && level(v) >= 0 && level(v) <= 100);
  // Shipped uncalibrated defaults cannot report a plausible percentage.
  assert(std::isnan(estimated_pellet_level(0.444f, 0, 0, 4, 30)));
  assert(std::isnan(estimated_pellet_level(0.444f, 0.444f, 23, 30, 4)));
  assert(std::isnan(estimated_pellet_level(0.444f, 0.444f, 23, 4, 4)));
  assert(std::isnan(estimated_pellet_level(0.444f, NAN, 23, 4, 30)));
  assert(std::isnan(estimated_pellet_level(0.444f, 0.444f, 35, 4, 30)));
  // Calibrated geometric endpoints within the sensor range clamp normally.
  assert(estimated_pellet_level(2.0f, 0.444f, 23, 6, 25) == 100);
  assert(estimated_pellet_level(0.35f, 0.444f, 23, 6, 25) == 0);
  HopperLevelFilter filter;
  float output = 123.0f;
  assert(filter.update(NAN, 0, output) && std::isnan(output));
  assert(!filter.update(NAN, 1000, output));
  assert(filter.update(50.2f, 3000, output) && output == 50.0f);
  // A brief reflection and sub-percent noise must not chatter or drag the
  // median upward. No ordinary update may publish inside the 30 s interval.
  for (uint32_t sec = 1; sec <= 30; ++sec) {
    float sample = sec == 15 ? 99.0f : (sec % 2 ? 49.8f : 50.2f);
    assert(!filter.update(sample, 3000 + sec * 1000, output));
  }
  for (uint32_t sec = 31; sec < 60; ++sec)
    assert(!filter.update(48.2f, 3000 + sec * 1000, output));
  assert(filter.update(48.2f, 63000, output) && output == 48.0f);
  // Opening and closing the lid clears all old samples, even inside the
  // reporting interval: a refill must not blend into the previous low level.
  assert(filter.update(NAN, 64000, output) && std::isnan(output));
  assert(filter.update(100.0f, 67000, output) && output == 100.0f);
  assert(filter.update(level(0.0f), 68000, output) && std::isnan(output));
  assert(filter.update(0.0f, 69000, output) && output == 0.0f);
  // Reporting cadence remains correct across the millis() rollover.
  HopperLevelFilter wrap_filter;
  assert(wrap_filter.update(80.0f, before_wrap, output) && output == 80.0f);
  for (uint32_t sec = 1; sec < 30; ++sec)
    assert(!wrap_filter.update(79.0f, before_wrap + sec * 1000, output));
  assert(wrap_filter.update(79.0f, before_wrap + 30000, output) && output == 79.0f);

  HopperDiagnosticFilter diagnostic;
  assert(diagnostic.update(0.6f, 100, output) && output == 0.6f);
  assert(!diagnostic.update(0.7f, 15099, output));
  assert(diagnostic.update(0.7f, 15100, output) && output == 0.7f);
  assert(diagnostic.update(NAN, 15101, output) && std::isnan(output));
  assert(!diagnostic.update(NAN, 15102, output));
  assert(diagnostic.update(1.7f, 18101, output) && output == 1.7f);
  assert(!diagnostic.update(1.6f, 18102, output));
  HopperDiagnosticFilter diagnostic_wrap;
  assert(diagnostic_wrap.update(1.0f, before_wrap, output));
  assert(!diagnostic_wrap.update(0.9f, before_wrap + 14999, output));
  assert(diagnostic_wrap.update(0.9f, before_wrap + 15000, output));
  std::cout << "Stove reporting tests passed\n";
}
