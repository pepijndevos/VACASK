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
        TIMEOUT 20
    )
endmacro()

# Create directory for manually running tests from vscode
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/manual_run")

foreach(item IN ITEMS ${TEST_DEPENDENCIES})
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
prepare_netlist_test(test_hb6.sim)
prepare_netlist_test(test_hbac1.sim)
prepare_netlist_test(test_pss1.sim)
prepare_netlist_test(test_pss2.sim)
prepare_netlist_test(test_pss3.sim)
prepare_netlist_test(test_pssosc1.sim)
prepare_netlist_test(test_pssosc2.sim)
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
prepare_netlist_test(test_behavnoise.sim)
