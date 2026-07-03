// daisy_api.h -- Daisy-specific API extensions on top of the BMI layer.
//
// DaisyAPI inherits BMI (pure BMI 2.0) and adds methods that are
// specific to Daisy coupling but are not part of the BMI standard.
// daisy_api.h -- Daisy-specific extensions on top of the BMI interface.indings.cpp.
//
// Adding a new non-BMI method:
//   1. Declare it here.
//   2. Implement it in daisy_api.C (calling ctrl()).
//   3. Add a .def() in api_bindings.cpp.

#ifndef DAISY_API_H
#define DAISY_API_H

#include <tuple>
#include <vector>
#include "programs/bmi.h"

class DaisyAPI : public BMI
{
public:
  // ===== PERTURBATION / Sy ESTIMATION =====

  /**
   * Estimate GW sensitivity by running two replay Richards solves (A at real
   * GW, B at GW+dh_cm) from the same t=0 initial state with daily-average
   * S_sum.  Returns theta_B - theta_A so replay artifacts cancel.
   *
   * Daisy is always restored to the real post-day result via a RAII guard.
   *
   * The caller computes Sy directly:
   *   delta_theta, _, _ = api.perturbation_tick(dh_cm)
   *   Sy = delta_theta / dh_cm          # no theta_B subtraction needed
   *
   * @param dh_cm  GW table perturbation in cm (default 1 cm, upward = positive)
   * @param col    Column index (default 0; multi-column not yet supported)
   * @return       tuple(delta_theta [-], flux_mm_d [mm/day], h_C [cm])
   *               — all arrays have length == number of soil layers
   */
  std::tuple<std::vector<double>, std::vector<double>, std::vector<double>>
    perturbation_tick(double dh_cm = 1.0, double dt_days = 1.0, unsigned int col = 0u);
};

#endif // DAISY_API_H
