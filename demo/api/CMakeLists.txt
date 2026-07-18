add_executable(demo1
    demo1.cpp
)

set_target_properties(demo1 PROPERTIES OUTPUT_NAME "demo1")
add_dependencies(demo1 simlib)

if (${SIM_PLATFORM} STREQUAL "Windows")
    target_link_options(demo1 PUBLIC -static)
endif()

add_executable(demo_netlistrs demo_netlistrs.cpp)
target_link_libraries(demo_netlistrs PRIVATE simlib netlist_cxx_bridge ${SIM_EXE_LIBRARIES})

if (${SIM_PLATFORM} STREQUAL "Windows")
    target_link_options(demo_netlistrs PUBLIC -static)
endif()

# OSDI models live in the VACASK in-tree build directory.
# No username-specific path: CMAKE_SOURCE_DIR is the root of this source tree.
set(VACASK_MOD_DIR_DEFAULT "${CMAKE_SOURCE_DIR}/build/lib/vacask/mod")
target_compile_definitions(demo_netlistrs PRIVATE
    VACASK_MOD_DIR="${VACASK_MOD_DIR_DEFAULT}")

add_test(NAME demo_netlistrs
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/rc.scs"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs PROPERTIES WORKING_DIRECTORY "/tmp")

add_test(NAME demo_netlistrs_include
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/rc_inc.scs"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_include PROPERTIES WORKING_DIRECTORY "/tmp")

add_test(NAME demo_netlistrs_badinc
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/rc_badinc.scs"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_badinc PROPERTIES
    WORKING_DIRECTORY "/tmp"
    WILL_FAIL TRUE)

add_test(NAME demo_netlistrs_nested
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/rc_nested.scs"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_nested PROPERTIES WORKING_DIRECTORY "/tmp")

# Fix 4: unique working directories so concurrent ctest -j runs don't race on tran1.raw.
# Each circuit gets its own /tmp/vacask_<name>/ dir created by a mkdir fixture test.

add_test(NAME vacask_mkdir_spice_rc
         COMMAND ${CMAKE_COMMAND} -E make_directory /tmp/vacask_spice_rc)
set_tests_properties(vacask_mkdir_spice_rc PROPERTIES
    FIXTURES_SETUP vacask_spice_rc_dir)

add_test(NAME demo_netlistrs_spice_rc
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/spice_rc.cir"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_spice_rc PROPERTIES
    WORKING_DIRECTORY "/tmp/vacask_spice_rc"
    FIXTURES_REQUIRED vacask_spice_rc_dir
    FIXTURES_SETUP vacask_spice_rc_raw)

# Each test writes tran1.raw into its own working directory (Fix 4: no race).
set(VACASK_SPICE_RC_RAW "/tmp/vacask_spice_rc/tran1.raw")

add_test(NAME demo_netlistrs_spice_rc_ngspice
         COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/ngspice_compare.sh"
                 "${CMAKE_CURRENT_SOURCE_DIR}/spice_rc.cir"
                 "${VACASK_SPICE_RC_RAW}")
set_tests_properties(demo_netlistrs_spice_rc_ngspice PROPERTIES
    WORKING_DIRECTORY "/tmp"
    SKIP_RETURN_CODE 77
    FIXTURES_REQUIRED vacask_spice_rc_raw)

# ── Diode half-wave rectifier (Task 6) ─────────────────────────────────────
add_test(NAME vacask_mkdir_spice_diode
         COMMAND ${CMAKE_COMMAND} -E make_directory /tmp/vacask_spice_diode)
set_tests_properties(vacask_mkdir_spice_diode PROPERTIES
    FIXTURES_SETUP vacask_spice_diode_dir)

add_test(NAME demo_netlistrs_spice_diode
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/spice_diode.cir"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_spice_diode PROPERTIES
    WORKING_DIRECTORY "/tmp/vacask_spice_diode"
    FIXTURES_REQUIRED vacask_spice_diode_dir
    FIXTURES_SETUP vacask_spice_diode_raw)

set(VACASK_SPICE_DIODE_RAW "/tmp/vacask_spice_diode/tran1.raw")

# ngspice comparison: VACASK diode vs ngspice built-in diode on the same netlist.
# Tolerance is wider than passive RC (model families differ) but both should
# show ~0.6-0.7 V forward drop and near-zero reverse.  Exits 77 (SKIP) if
# ngspice is absent so CI without ngspice still builds green.
#
# Sample at quarter-period peaks of the 1 kHz SIN source (T/4=0.25ms), so the
# diode is conducting at every sample point (V_source ≈ +2 V → V(out) ≈ 1.0 V).
# Using zero-crossings (multiples of 1 ms) would give V(out)≈0 regardless of
# whether the diode works — this ensures the comparison is meaningful.
#
# Tolerance: 30 mV abs / 3% rel.  At forward-bias peaks the VACASK diode.osdi
# model diverges from ngspice's built-in by up to ~23 mV (verified empirically).
# The two model families (Shockley exponential implementations differ in their
# limit/smoothing details) so sub-5 mV agreement is not expected.  30 mV still
# catches gross errors (wrong model, wrong polarity, not conducting at all).
set(DIODE_SAMPLE_TS "0.25e-3,1.25e-3,2.25e-3,3.25e-3,4.25e-3")
add_test(NAME demo_netlistrs_spice_diode_ngspice
         COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/ngspice_compare.sh"
                 "${CMAKE_CURRENT_SOURCE_DIR}/spice_diode.cir"
                 "${VACASK_SPICE_DIODE_RAW}"
                 "${DIODE_SAMPLE_TS}"
                 "30e-3"
                 "0.03")
set_tests_properties(demo_netlistrs_spice_diode_ngspice PROPERTIES
    WORKING_DIRECTORY "/tmp"
    SKIP_RETURN_CODE 77
    FIXTURES_REQUIRED vacask_spice_diode_raw)

# ── SPICE subckt X-call + .subckt (Task 7) ─────────────────────────────────
# Step 1 (failing): X1 is skipped because SubcktCall adapter is not yet implemented.
# Step 4 (passing): X1:R1 r=2000 verified (rval override from 1k to 2k).
add_test(NAME demo_netlistrs_spice_subckt
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/spice_subckt.cir"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_spice_subckt PROPERTIES
    WORKING_DIRECTORY "/tmp")

# ── VCVS controlled source (Task 7) ─────────────────────────────────────────
add_test(NAME vacask_mkdir_spice_vcvs
         COMMAND ${CMAKE_COMMAND} -E make_directory /tmp/vacask_spice_vcvs)
set_tests_properties(vacask_mkdir_spice_vcvs PROPERTIES
    FIXTURES_SETUP vacask_spice_vcvs_dir)

add_test(NAME demo_netlistrs_spice_vcvs
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/spice_vcvs.cir"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_spice_vcvs PROPERTIES
    WORKING_DIRECTORY "/tmp/vacask_spice_vcvs"
    FIXTURES_REQUIRED vacask_spice_vcvs_dir
    FIXTURES_SETUP vacask_spice_vcvs_raw)

set(VACASK_SPICE_VCVS_RAW "/tmp/vacask_spice_vcvs/tran1.raw")

# ngspice comparison: VACASK vcvs vs ngspice E-source on the same netlist.
# V(2) = 2*V(1) at all times (ideal vcvs).  Sample at quarter-period peaks
# (V(1)=±1 → V(2)=±2) for a sensitive comparison.
#
# Tolerance is wider than the passive RC test because VACASK's adaptive
# timestep on a smooth sine wave produces ~150 output points over 10 ms
# (avg ~66 µs/step) while ngspice uses a fixed 1 µs step.  Linear
# interpolation of a 1 kHz sine with 66 µs steps gives up to ~80 mV
# interpolation error at peaks — this is a raw-file artifact, not a physics
# error.  100 mV / 5 % still catches a wrong gain (factor-of-2 error → 1 V
# residual >> 100 mV) while tolerating the interpolation noise.
set(VCVS_SAMPLE_TS "0.25e-3,0.75e-3,1.25e-3,1.75e-3,2.25e-3")
add_test(NAME demo_netlistrs_spice_vcvs_ngspice
         COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/ngspice_compare.sh"
                 "${CMAKE_CURRENT_SOURCE_DIR}/spice_vcvs.cir"
                 "${VACASK_SPICE_VCVS_RAW}"
                 "${VCVS_SAMPLE_TS}"
                 "100e-3"
                 "0.05")
set_tests_properties(demo_netlistrs_spice_vcvs_ngspice PROPERTIES
    WORKING_DIRECTORY "/tmp"
    SKIP_RETURN_CODE 77
    FIXTURES_REQUIRED vacask_spice_vcvs_raw)

# ── NMOS resistor-load stage (Task 6) ──────────────────────────────────────
# Verifies: Analysis OK + m1 present in hierarchy with l=3.5e-07.
add_test(NAME demo_netlistrs_spice_mos
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/spice_mos.cir"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_spice_mos PROPERTIES
    WORKING_DIRECTORY "/tmp")

# ── CCCS F-source E2E (Fix 3) ────────────────────────────────────────────────
# Verifies: F1.gain=5 (ctlinst=Vsense, ctlnode=flow(br) default confirmed).
# V(2) = -5V (DC, steady): ngspice comparison at default sample times.
add_test(NAME vacask_mkdir_spice_cccs
         COMMAND ${CMAKE_COMMAND} -E make_directory /tmp/vacask_spice_cccs)
set_tests_properties(vacask_mkdir_spice_cccs PROPERTIES
    FIXTURES_SETUP vacask_spice_cccs_dir)

add_test(NAME demo_netlistrs_spice_cccs
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/spice_cccs.cir"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_spice_cccs PROPERTIES
    WORKING_DIRECTORY "/tmp/vacask_spice_cccs"
    FIXTURES_REQUIRED vacask_spice_cccs_dir
    FIXTURES_SETUP vacask_spice_cccs_raw)

set(VACASK_SPICE_CCCS_RAW "/tmp/vacask_spice_cccs/tran1.raw")

# ngspice comparison: V(2)=-5V constant (DC CCCS circuit).
# Tight tolerance: 5mV abs / 2% rel (pure DC).
add_test(NAME demo_netlistrs_spice_cccs_ngspice
         COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/ngspice_compare.sh"
                 "${CMAKE_CURRENT_SOURCE_DIR}/spice_cccs.cir"
                 "${VACASK_SPICE_CCCS_RAW}")
set_tests_properties(demo_netlistrs_spice_cccs_ngspice PROPERTIES
    WORKING_DIRECTORY "/tmp"
    SKIP_RETURN_CODE 77
    FIXTURES_REQUIRED vacask_spice_cccs_raw)

# ── SPICE .include resolution (Fix 2) ────────────────────────────────────────
# spice_inc.cir includes spice_models.spice which defines dmod.
# Test: modelParameter("dmod","is") == "1e-14" (resolved from included file).
add_test(NAME vacask_mkdir_spice_inc
         COMMAND ${CMAKE_COMMAND} -E make_directory /tmp/vacask_spice_inc)
set_tests_properties(vacask_mkdir_spice_inc PROPERTIES
    FIXTURES_SETUP vacask_spice_inc_dir)

add_test(NAME demo_netlistrs_spice_inc
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/spice_inc.cir"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_spice_inc PROPERTIES
    WORKING_DIRECTORY "/tmp/vacask_spice_inc"
    FIXTURES_REQUIRED vacask_spice_inc_dir)

# ── Spectre subckt with nested SPICE block (Fix 1) ───────────────────────────
# NOTE: The Rust parser does not currently support `simulator lang=spice` inside
# a Spectre subckt body (produces parse errors). The C++ fillSubDef fix (iterating
# spice_blocks) is a latent/preemptive fix for when the parser gains that capability.
# No test is registered here; the fix is covered by code review + report.
# When the parser supports this, re-enable with a .scs file using the nested syntax.

# ── Warn-skip test (Minor 7) ─────────────────────────────────────────────────
# B1 (Behavioral) is warned + skipped; R1/R2 divider runs cleanly.
# PASS_REGULAR_EXPRESSION verifies the WARNING appears; exit 0 confirms circuit runs.
add_test(NAME demo_netlistrs_spice_warnonly
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/spice_warnonly.cir"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_spice_warnonly PROPERTIES
    WORKING_DIRECTORY "/tmp"
    PASS_REGULAR_EXPRESSION "WARNING.*Behavioral")

target_link_libraries(
    demo1
    simlib
    ${SIM_EXE_LIBRARIES}
)


add_executable(demo2
    demo2.cpp
)

set_target_properties(demo2 PROPERTIES OUTPUT_NAME "demo2")
add_dependencies(demo2 simlib)

if (${SIM_PLATFORM} STREQUAL "Windows")
    target_link_options(demo2 PUBLIC -static)
endif()

target_link_libraries(
    demo2
    simlib
    ${SIM_EXE_LIBRARIES}
)


add_executable(demo3
    demo3.cpp
)

set_target_properties(demo3 PROPERTIES OUTPUT_NAME "demo3")
add_dependencies(demo3 simlib)

if (${SIM_PLATFORM} STREQUAL "Windows")
    target_link_options(demo3 PUBLIC -static)
endif()

target_link_libraries(
    demo3
    simlib
    ${SIM_EXE_LIBRARIES}
)


add_executable(demo4
    demo4.cpp
)

set_target_properties(demo4 PROPERTIES OUTPUT_NAME "demo4")
add_dependencies(demo4 simlib)

if (${SIM_PLATFORM} STREQUAL "Windows")
    target_link_options(demo4 PUBLIC -static)
endif()

target_link_libraries(
    demo4
    simlib
    ${SIM_EXE_LIBRARIES}
)


add_executable(demo5
    demo5.cpp
)

set_target_properties(demo5 PROPERTIES OUTPUT_NAME "demo5")
add_dependencies(demo5 simlib)

if (${SIM_PLATFORM} STREQUAL "Windows")
    target_link_options(demo5 PUBLIC -static)
endif()

target_link_libraries(
    demo5
    simlib
    ${SIM_EXE_LIBRARIES}
)