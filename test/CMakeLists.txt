# Testing - requires a working installation of python3 and openvaf

set(TEST_DEPENDENCIES
    diocalc.py
    ../demo/ihp-sg13g2/.vacaskrc.toml
)

# macro that sets up a test
macro(prepare_netlist_test simfile)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${simfile}")
    foreach(item IN ITEMS ${TEST_DEPENDENCIES})
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${item} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${simfile}")
    endforeach()

    if(${ARGC} GREATER 1)
        add_test(NAME "${simfile}"
            COMMAND ${CMAKE_COMMAND}
                "-DSIMULATOR=$<TARGET_FILE:sim>"
                "-DNETLIST=${CMAKE_CURRENT_SOURCE_DIR}/${simfile}"
                "-DEXPECTED_WARNING=${ARGV1}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/check_warning.cmake"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${simfile}"
        )
    else()
        add_test(NAME "${simfile}"
            COMMAND $<TARGET_FILE:sim> -dp "${CMAKE_CURRENT_SOURCE_DIR}/${simfile}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${simfile}"
        )
    endif()
    
    # message("Adding netlist test ${simfile}")
    
    # PYTHONPATH points to the binary test directory
    # Each test has a subdirectory there
    # Dependencies (.py files like diocalc.py) will be copied there
    set_tests_properties(
        "${simfile}" PROPERTIES 
        ENVIRONMENT 
        "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR};SIM_TEST=yes"
        TIMEOUT 20
    )
endmacro()

macro(prepare_failure_test simfile expected_error)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${simfile}")
    add_test(NAME "${simfile}"
        COMMAND ${CMAKE_COMMAND}
            "-DSIMULATOR=$<TARGET_FILE:sim>"
            "-DNETLIST=${CMAKE_CURRENT_SOURCE_DIR}/${simfile}"
            "-DEXPECTED_ERROR=${expected_error}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_failure.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${simfile}"
    )
    set_tests_properties("${simfile}" PROPERTIES TIMEOUT 20)
endmacro()

# macro that sets up a test built from a C++ source file, linked against
# the simulator library. The test binary is run in a subdirectory named
# after the source file (without its extension); a nonzero exit status
# (in particular 1) is treated by ctest as a failed test, same as any
# other test.
macro(prepare_binary_test testfile)
    get_filename_component(testname ${testfile} NAME_WE)

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${testname}")

    add_executable(${testname} ${testfile})
    add_dependencies(${testname} simlib)
    # Build the binary into its own directory - the default output location
    # (CMAKE_CURRENT_BINARY_DIR, same as the target name) would collide with
    # the same-named working directory created above.
    set_target_properties(${testname} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )

    if (${SIM_PLATFORM} STREQUAL "Windows")
        target_link_options(${testname} PUBLIC -static)
    endif()

    target_link_libraries(
        ${testname}
        simlib
        ${SIM_EXE_LIBRARIES}
    )

    add_test(NAME "${testname}"
        COMMAND $<TARGET_FILE:${testname}>
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${testname}"
    )

    # Same environment/timeout convention as prepare_netlist_test()
    set_tests_properties(
        "${testname}" PROPERTIES
        ENVIRONMENT
        "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR};SIM_TEST=yes"
        TIMEOUT 20
    )
endmacro()

# Create directory for manually running tests from vscode
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/manual_run")

foreach(item IN ITEMS ${TEST_DEPENDENCIES})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${item} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/manual_run")
endforeach()

prepare_binary_test(test_blaslapack.cpp)
prepare_binary_test(test_densematrix.cpp)
prepare_binary_test(test_integcoffs.cpp)
prepare_binary_test(test_hbcore.cpp)

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
prepare_netlist_test(test_hb6.sim)
prepare_netlist_test(test_hbac1.sim)
prepare_netlist_test(test_pss1.sim)
prepare_netlist_test(test_pss2.sim)
prepare_netlist_test(test_pss3.sim)
prepare_netlist_test(test_pssosc1.sim)
prepare_netlist_test(test_pssosc2.sim)
set_tests_properties(test_pssosc2.sim PROPERTIES LABELS requires_ihp_pdk)
prepare_netlist_test(test_pssosc3.sim)
prepare_netlist_test(test_pssosc4.sim)
prepare_netlist_test(test_pssosc5.sim)
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
prepare_netlist_test(test_behav.sim)
prepare_netlist_test(test_behavsym.sim)
prepare_netlist_test(test_behavop.sim)
prepare_netlist_test(test_behavnoise.sim)
prepare_netlist_test(test_hbnoise1.sim)
prepare_netlist_test(test_hbnoise2.sim)
# Foreign-format include dispatch: a native deck that pulls its devices from a
# SPICE-dialect file via `include`. Only meaningful when the Rust parser is
# compiled in.
if(CADNIP_PARSERS)
    prepare_netlist_test(test_spice_include.sim)
    prepare_netlist_test(test_spice_include_section.sim)
    prepare_netlist_test(test_lang_include.sim)
    prepare_netlist_test(test_lang_override.sim)
    prepare_netlist_test(test_spectre_rc.sim)
    prepare_netlist_test(test_spectre_include.sim)
    prepare_netlist_test(test_spectre_subckt.sim)
    prepare_failure_test(test_spectre_bad_include.sim "cannot open include")
    prepare_netlist_test(test_spice_rc.sim)
    prepare_netlist_test(test_spice_diode.sim)
    prepare_netlist_test(test_spice_diode_params.sim)
    prepare_netlist_test(test_spice_subckt.sim)
    prepare_netlist_test(test_spice_vcvs.sim)
    prepare_netlist_test(test_spice_cccs.sim)
    prepare_netlist_test(test_spice_nested_include.sim)
    prepare_netlist_test(test_spice_res_model.sim)
    prepare_netlist_test(test_spice_regressions.sim)
    prepare_netlist_test(test_spice_mos.sim)
    prepare_netlist_test(test_spice_bsim4_version.sim "version = \"4.80\"")
    add_test(NAME test_spice_bsim4_unknown_version_warning.sim
        COMMAND ${CMAKE_COMMAND}
            "-DSIMULATOR=$<TARGET_FILE:sim>"
            "-DNETLIST=${CMAKE_CURRENT_SOURCE_DIR}/test_spice_bsim4_version.sim"
            "-DEXPECTED_WARNING=BSIM4 version '9.9' is not supported.*falling back to 4.8.3"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_warning.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_spice_bsim4_version.sim")
    set_tests_properties(test_spice_bsim4_unknown_version_warning.sim PROPERTIES
        ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR};SIM_TEST=yes" TIMEOUT 20 LABELS "warning")
    set_tests_properties(test_spice_bsim4_version.sim test_spice_bsim4_unknown_version_warning.sim
        PROPERTIES RESOURCE_LOCK spice_bsim4_version)
    prepare_netlist_test(test_spice_binned_mos.sim)
    prepare_netlist_test(test_spice_binned_mos_section.sim)
    prepare_netlist_test(test_spice_binned_mos_indirect.sim)
    prepare_netlist_test(test_spice_binned_mos_boundary.sim)
    prepare_failure_test(test_spice_binned_mos_below_min.sim "Master 'm_nboundary_model' not found")
    prepare_failure_test(test_spice_binned_mos_at_max.sim "Master 'm_nboundary_model' not found")
    prepare_netlist_test(test_spice_bsource.sim)
    prepare_netlist_test(test_spice_behavioral_res.sim)
    prepare_netlist_test(test_spice_bjt.sim)
    prepare_netlist_test(test_spice_model_subckt_namespace.sim)
    prepare_netlist_test(test_spice_mfactor.sim)
    prepare_netlist_test(test_spice_temper.sim)
    prepare_netlist_test(test_spice_pwr.sim)
    prepare_netlist_test(test_spice_temper_shadow.sim "declares a parameter named 'temper'")
    set_tests_properties(test_spice_temper_shadow.sim PROPERTIES LABELS "warning")
    prepare_netlist_test(test_spice_unknown_param.sim)
    prepare_netlist_test(test_spice_unknown_param_warn.sim "Warning, parameter 'dcap' not found")
    set_tests_properties(test_spice_unknown_param_warn.sim PROPERTIES LABELS "warning")
    prepare_failure_test(test_spice_unknown_param_error.sim "Parameter 'dcap' not found")
    prepare_netlist_test(test_spice_warnonly.sim "WARNING: SPICE device.*MutualInductor.*skipped")
    set_tests_properties(test_spice_warnonly.sim PROPERTIES LABELS "warning")

endif()
prepare_netlist_test(test_behavexpr.sim)
prepare_netlist_test(test_delay.sim)
prepare_netlist_test(test_delayvar.sim)
prepare_netlist_test(test_delayhb.sim)
prepare_netlist_test(test_delayhbac.sim)
