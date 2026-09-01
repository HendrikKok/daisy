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
   * Estimate GW sensitivity by running two Richards replays (A at real GW,
   * B at GW+dh_cm) from the same t=0 state.  Both follow the recorded real-run
   * timestep schedule and use identical per-step S_sum, ponding, temperature
   * and ice state.  Returns theta_B - theta_A so replay artifacts cancel.
   *
   * Daisy is always restored to the real post-day result via a RAII guard.
   *
   * The caller computes Sy directly:
   *   delta_theta, _, _ = api.perturbation_tick(dh_cm)
    *   Sy = sum(delta_theta[i] * dz_cm[i]) / dh_cm
    *                                      # no real-theta subtraction needed
   *
   * @param dh_cm    GW table perturbation in cm (default 1 cm, upward = positive)
   * @param do_reset If true (default), cells newly submerged by the perturbed
   *                 GW table are forced to hydrostatic equilibrium before the
   *                 replay; if false, the perturbed replay is left to Richards
   *                 alone (diagnostic use, e.g. isolating this forcing's effect
   *                 on the resulting Sy estimate).
   * @param col      Column index (default 0; multi-column not yet supported)
    * @return       tuple(delta_theta [-], average flux_B [mm/day], h_C [cm])
   *               — all arrays have length == number of soil layers
   */
  std::tuple<std::vector<double>, std::vector<double>, std::vector<double>>
    perturbation_tick(double dh_cm = 1.0, double dt_days = 1.0, bool do_reset = true, unsigned int col = 0u);

  // ===== SATURATED PRESSURE RESET =====
  /**
   * Set saturated cells (hydrostatic h >= 0) to correct h and theta, then
   * reset h_old.  Unsaturated cells are untouched.
   *
   * Call after set_value("groundwater__depth", ...) and before the next
   * update_until().  For loam/clay the diffusion timescale L^2*C/K can be
   * days-to-months, so Richards cannot propagate the new GWT within one tick.
   * Without this call, Sy is severely underestimated and MODFLOW heads drift.
   *
   * The returned delta_W [cm] is diagnostic only: it equals the water already
   * accounted for in MODFLOW's head change (no correction to MODFLOW needed).
   *
   * @param col  Column index (default 0).
   * @return     Column-integrated delta_W [cm] (positive = water added).
   */
  double reset_saturated_pressure (unsigned int col = 0u);

  // ===== RICHARDS SOLVER FAILURE COUNTER =====
  /**
   * Total (never reset) count of matrix-water Richards solver fallbacks
   * for the column, accumulated over the whole simulation so far.
   *
   * The internal per-tick failure flag is reset every tick, so it cannot be
   * queried after the fact.  This counter only ever increases, so callers
   * can diff it before/after an update_until() call to detect whether that
   * specific step required a fallback to a lower-fidelity solver (e.g. to
   * decide whether to trust a subsequent Sy estimate or keep the previous
   * value instead).
   *
   * @param col  Column index (default 0).
   * @return     Cumulative failure count for the column.
   */
  size_t water_fail_count (unsigned int col = 0u) const;
};

#endif // DAISY_API_H
