// api_bindings.cpp -- pybind11 bindings for BMI and DaisyAPI.
//
// Exposes both classes to Python as the module "daisy_bmi".
//   BMI  — pure BMI 2.0 standard interface.
//   DaisyAPI  — inherits BMI, adds Daisy-specific extensions
//               (perturbation_tick, etc.).
//
// Python usage:
//   from daisy import API
//   api = API()
//   api.initialize("myconfig.dai")
//   while api.get_current_time() < 365.0:
//       api.set_value("groundwater__depth", 150.0)
//       api.update()
//       theta_B = api.get_value_array("soil_water__content")
//       theta_C, flux_C, h_C = api.perturbation_tick(1.0)
//   api.finalize()

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "programs/bmi.h"
#include "programs/daisy_api.h"

namespace py = pybind11;

PYBIND11_MODULE(daisy_bmi, m)
{
  m.doc() = R"pbdoc(
    daisy_bmi - BMI 2.0 interface to the Daisy soil-crop-atmosphere model
    =======================================================================

    Example
    -------
    import daisy_bmi

    bmi = daisy_bmi.BMI()
    bmi.initialize("myconfig.dai")

    print("Component:", bmi.get_component_name())
    print("Time step:", bmi.get_time_step(), bmi.get_time_units())
    print("Inputs:",    bmi.get_input_var_names())
    print("Outputs:",   bmi.get_output_var_names())

    while bmi.get_current_time() < 365.0:
        # Pull outputs
        recharge = bmi.get_value("soil_water__recharge_rate")
        et       = bmi.get_value("land_surface__evapotranspiration_rate")

        # Push inputs
        bmi.set_value("groundwater__depth", 150.0)  # 150 cm below surface

        bmi.update()

    bmi.finalize()
  )pbdoc";

  py::class_<BMI>(m, "BMI")

    .def(py::init<>(), "Create a new Daisy BMI instance")

    // --- Lifecycle ---
    .def("initialize",    &BMI::initialize,    py::arg("config_file"),
         "Initialize from a .dai config file")
    .def("update",        &BMI::update,
         "Advance model by one timestep")
    .def("update_until",  &BMI::update_until,  py::arg("time"),
         "Advance model until time (days since start)")
    .def("finalize",      &BMI::finalize,
         "Finalize and release all resources")

    // --- Information ---
    .def("get_component_name",    &BMI::get_component_name)
    .def("get_input_var_names",   &BMI::get_input_var_names)
    .def("get_output_var_names",  &BMI::get_output_var_names)

    // --- Time ---
    .def("get_start_time",    &BMI::get_start_time,
         "Simulation start time [days]")
    .def("get_end_time",      &BMI::get_end_time,
         "Simulation end time [days] (NaN = open-ended)")
    .def("get_current_time",  &BMI::get_current_time,
         "Current simulation time [days since start]")
    .def("get_time_step",     &BMI::get_time_step,
         "Current timestep size [days]")
    .def("get_time_units",    &BMI::get_time_units,
         "Time unit string, e.g. 'd'")

    // --- Variable metadata ---
    .def("get_var_type",      &BMI::get_var_type,     py::arg("name"))
    .def("get_var_units",     &BMI::get_var_units,    py::arg("name"))
    .def("get_var_itemsize",  &BMI::get_var_itemsize, py::arg("name"))
    .def("get_var_nbytes",    &BMI::get_var_nbytes,   py::arg("name"))
    .def("get_var_location",  &BMI::get_var_location, py::arg("name"))
    .def("get_var_grid",      &BMI::get_var_grid,     py::arg("name"))

    // --- Grid metadata ---
    .def("get_grid_rank",  &BMI::get_grid_rank, py::arg("grid"))
    .def("get_grid_size",  &BMI::get_grid_size, py::arg("grid"))
    .def("get_grid_type",  &BMI::get_grid_type, py::arg("grid"))

    // --- Get / Set (scalar convenience wrappers) ---
    .def("get_value",
         [](const BMI& self, const std::string& name) -> double {
           // Allocate the correct-sized buffer; array vars (e.g.
           // soil_water__recharge_rate) hold one value per soil layer.
           // Writing N elements into a 1-element stack variable is UB and
           // caused WinError 10054 crashes via stack corruption.
           int n = self.get_var_nbytes(name) / static_cast<int>(sizeof(double));
           std::vector<double> buf(n);
           self.get_value(name, buf.data());
           return buf.back(); // scalar = bottom-layer value, matches get_output_value
         },
         py::arg("name"),
         "Get a scalar output value by BMI variable name")

    .def("set_value",
         [](BMI& self, const std::string& name, double value) {
           self.set_value(name, &value);
         },
         py::arg("name"), py::arg("value"),
         "Set a scalar input value by BMI variable name")

    .def("get_value_array",
     [](const BMI& self, const std::string& name) {
       int n = self.get_var_nbytes(name) / static_cast<int>(sizeof(double));
       py::array_t<double> arr(n);
       self.get_value(name, arr.mutable_data());
       return arr;
     },
     py::arg("name"),
     "Get an array output value as a numpy array of doubles")

    .def("set_value_array",
     [](BMI& self, const std::string& name, py::array_t<double, py::array::c_style> arr) {
       self.set_value(name, arr.data());
     },
     py::arg("name"), py::arg("values"),
     "Set an array input value from a numpy array of doubles");

  // ---------------------------------------------------------------------------
  // DaisyAPI — inherits all BMI methods, adds Daisy-specific extensions.
  // This is the recommended class for Python coupling.
  // ---------------------------------------------------------------------------
  py::class_<DaisyAPI, BMI>(m, "API")
    .def(py::init<>(), "Create a Daisy API instance (BMI + extensions)")

    .def("perturbation_tick",
         &DaisyAPI::perturbation_tick,
         py::arg("dh_cm")    = 1.0,
         py::arg("dt_days")  = 1.0,
         py::arg("do_reset") = true,
         py::arg("col")      = 0u,
         R"pbdoc(
 Run matched per-substep Richards replays at the real and perturbed GW tables.
 Daisy is always restored to the real post-tick result (RAII guard).

 The step accumulator is cleared automatically after each call, so the next
 update_until() starts a fresh day.

 do_reset : bool, default True
     If True, cells newly submerged by the perturbed GW table are forced to
     hydrostatic equilibrium before the replay (equilibrate_saturated_zone).
     If False, the perturbed replay is left to Richards alone -- useful to
     test how much this forcing contributes to the resulting Sy estimate.

 Returns
 -------
 tuple(delta_theta, flux_mm_d, h_C)
      delta_theta : list[float]  theta_perturbed - theta_reference per layer [-]
      flux_mm_d   : list[float]  time-averaged perturbed flux per layer [mm/day]
      h_C         : list[float]  perturbed pressure head per layer [cm]

 Typical usage::

   api.update_until(t + 1.0)          # runs 24 internal hourly steps
   delta_theta, _, _ = api.perturbation_tick(dh_cm)
   Sy = sum(delta_theta[i] * dz_cm[i] for i in range(n)) / dh_cm
         )pbdoc")

    .def("reset_saturated_pressure",
         &DaisyAPI::reset_saturated_pressure,
         py::arg("col") = 0u,
         R"pbdoc(
 Set saturated cells (hydrostatic h >= 0) to correct pressure and theta,
 then reset h_old.  Unsaturated cells are untouched.

 Call after set_value("groundwater__depth", ...) and before the next
 update_until() for any non-sandy soil.  Without this, Richards cannot
 propagate the new GWT within one tick (diffusion timescale L^2*C/K can
 be days-to-months for loam/clay), causing underestimated Sy and MODFLOW
 head drift.

 The returned delta_W is diagnostic only: it represents the water already
 accounted for in MODFLOW's head change.  No correction to MODFLOW is needed.

 Returns
 -------
 float
     Column-integrated delta_W [cm].  Positive = water added to saturated
     cells.  Should converge to ~0 after initialisation in a closed system.
         )pbdoc")

    .def("water_fail_count",
         &DaisyAPI::water_fail_count,
         py::arg("col") = 0u,
         R"pbdoc(
 Cumulative (never reset) count of matrix-water Richards solver fallbacks
 for the column, accumulated over the whole simulation so far.

 Diff this value before and after an update_until() call to detect whether
 that specific step required a fallback to a lower-fidelity solver (e.g. to
 decide whether to trust a subsequent Sy estimate or keep the previous
 value instead).

 Returns
 -------
 int
     Cumulative fallback count for the column.
         )pbdoc");
}
