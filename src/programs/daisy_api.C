// daisy_api.C -- Implementation of DaisyAPI non-BMI extension methods.

#include "programs/daisy_api.h"
#include "daisy/daisy.h"
#include <stdexcept>

auto DaisyAPI::perturbation_tick (double dh_cm, double dt_days, bool do_reset, unsigned int col)
  -> std::tuple<std::vector<double>, std::vector<double>, std::vector<double>>
{
  if (col != 0u)
    throw std::invalid_argument (
      "perturbation_tick: col > 0 not yet supported");
  return daisy ().perturbation_tick (dh_cm, dt_days, do_reset);
}

double DaisyAPI::reset_saturated_pressure (unsigned int col)
{
  if (col != 0u)
    throw std::invalid_argument (
      "reset_saturated_pressure: col > 0 not yet supported");
  return daisy ().reset_saturated_pressure ();
}

size_t DaisyAPI::water_fail_count (unsigned int col) const
{
  if (col != 0u)
    throw std::invalid_argument (
      "water_fail_count: col > 0 not yet supported");
  return daisy ().water_fail_count ();
}
