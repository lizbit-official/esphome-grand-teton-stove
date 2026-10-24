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
  std::cout << "Stove reporting tests passed\n";
}
