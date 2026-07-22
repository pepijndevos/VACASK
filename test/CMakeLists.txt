# Testing - requires a working installation of python3 and openvaf

# Directory where the python script that runs a test can be found
# Use staging directory to keep source clean
set(PYTHON_LIB "${STAGED_LIBRARY_DIRECTORY}/python")
message("Python library path for testing: ${PYTHON_LIB}")

# macro that sets up a test
macro(prepare_netlist_test simfile)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${simfile}")
    add_test(NAME "${simfile}"
        COMMAND $<TARGET_FILE:sim> -dp "${CMAKE_CURRENT_SOURCE_DIR}/${simfile}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${simfile}"
    )
    
    # message("Adding netlist test ${simfile}")
    
    # PYTHONPATH points to the binary test directory
    # Each test has a subdirectory there
    # Dependencies (.py files like diocalc.py) will be copied there
    set_tests_properties(
        "${simfile}" PROPERTIES 
        ENVIRONMENT 
        "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR};SIM_TEST=yes"
        TIMEOUT 15
    )
endmacro()

# Create directory for manually running tests from vscode
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/manual_run")

set(TEST_DEPENDENCIES
    diocalc.py
)

foreach(item IN ITEMS ${TEST_DEPENDENCIES})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${item} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${item} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/manual_run")
endforeach()

prepare_netlist_test(test_op.sim)
prepare_netlist_test(test_opnl.sim)
prepare_netlist_test(test_opnodeset.sim)
prepare_netlist_test(test_opstore.sim)
prepare_netlist_test(test_opsweep.sim)
prepare_netlist_test(test_sweep.sim)
prepare_netlist_test(test_sweeppar.sim)
prepare_netlist_test(test_sweepvar.sim)
prepare_netlist_test(test_sweepvec.sim)
prepare_netlist_test(test_dcinc.sim)
prepare_netlist_test(test_dcxf.sim)
prepare_netlist_test(test_ac.sim)
prepare_netlist_test(test_acxf.sim)
prepare_netlist_test(test_acstb.sim)
prepare_netlist_test(test_acsp.sim)
prepare_netlist_test(test_tran.sim)
prepare_netlist_test(test_tranic.sim)
prepare_netlist_test(test_trannoise1.sim)
prepare_netlist_test(test_trannoise2.sim)
prepare_netlist_test(test_trannoise3.sim)
prepare_netlist_test(test_trannoise4.sim)
prepare_netlist_test(test_noise.sim)
prepare_netlist_test(test_hb1.sim)
prepare_netlist_test(test_hb2.sim)
prepare_netlist_test(test_hb3.sim)
prepare_netlist_test(test_hb4.sim)
prepare_netlist_test(test_hb5.sim)
prepare_netlist_test(test_build.sim)
prepare_netlist_test(test_cblock.sim)
prepare_netlist_test(test_cblocksweep.sim)
prepare_netlist_test(test_visrc.sim)
prepare_netlist_test(test_viwfm.sim)
prepare_netlist_test(test_vipwl.sim)
prepare_netlist_test(test_ctlsrc.sim)
prepare_netlist_test(test_resistor.sim)
prepare_netlist_test(test_capacitor.sim)
prepare_netlist_test(test_inductor.sim)
prepare_netlist_test(test_mutual.sim)
prepare_netlist_test(test_diode.sim)
prepare_netlist_test(test_inverter.sim)
prepare_netlist_test(test_optpar.sim)
prepare_netlist_test(test_mc.sim)

# Foreign-format include dispatch: a native deck that pulls its devices from a
# SPICE-dialect file via `include`. Only meaningful when the Rust parser is
# compiled in.
if(VACASK_WITH_SPICE)
    prepare_netlist_test(test_spice_include.sim)
    prepare_netlist_test(test_spice_include_section.sim)
    prepare_netlist_test(test_lang_include.sim)

    # Sky130 ngspice PDK include E2E: a native deck that includes the raw ngspice
    # Sky130 model sources directly (no Cadnip.jl conversion) and solves an nfet
    # operating point through the vacask binary. Registered only when the PDK
    # checkout is present, so CI without it stays green. The PDK path is baked
    # into a generated deck via configure_file (keeps the committed .sim.in
    # machine-independent).
    set(SKY130_CONTINUOUS "${SKY130_PDK_ROOT}/combined_models/continuous")
    if(EXISTS "${SKY130_CONTINUOUS}/models_fet/sky130_fd_pr__nfet_01v8.spice")
        set(_sky130_test test_sky130_nfet_include.sim)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${_sky130_test}")
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/test_sky130_nfet_include.sim.in"
            "${CMAKE_CURRENT_BINARY_DIR}/${_sky130_test}/${_sky130_test}" @ONLY)
        add_test(NAME "${_sky130_test}"
            COMMAND $<TARGET_FILE:sim> -dp "${CMAKE_CURRENT_BINARY_DIR}/${_sky130_test}/${_sky130_test}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${_sky130_test}")
        set_tests_properties("${_sky130_test}" PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR};SIM_TEST=yes"
            TIMEOUT 30)
    else()
        message(STATUS "Skipping test_sky130_nfet_include (Sky130 PDK not found at ${SKY130_CONTINUOUS})")
    endif()
endif()
