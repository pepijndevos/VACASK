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

add_test(NAME demo_netlistrs_spice_rc
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/spice_rc.cir"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_spice_rc PROPERTIES
    WORKING_DIRECTORY "/tmp"
    FIXTURES_SETUP vacask_spice_rc_raw)

# The transient analysis in demo_netlistrs is named "tran1", so VACASK writes
# tran1.raw into the working directory (/tmp) of demo_netlistrs_spice_rc.
set(VACASK_SPICE_RC_RAW "/tmp/tran1.raw")

add_test(NAME demo_netlistrs_spice_rc_ngspice
         COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/ngspice_compare.sh"
                 "${CMAKE_CURRENT_SOURCE_DIR}/spice_rc.cir"
                 "${VACASK_SPICE_RC_RAW}")
set_tests_properties(demo_netlistrs_spice_rc_ngspice PROPERTIES
    WORKING_DIRECTORY "/tmp"
    SKIP_RETURN_CODE 77
    FIXTURES_REQUIRED vacask_spice_rc_raw)

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