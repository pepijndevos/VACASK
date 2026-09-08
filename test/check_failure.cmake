execute_process(
    COMMAND "${SIMULATOR}" -dp "${NETLIST}"
    RESULT_VARIABLE simulator_status
    OUTPUT_VARIABLE simulator_stdout
    ERROR_VARIABLE simulator_stderr
)

set(simulator_output "${simulator_stdout}\n${simulator_stderr}")
message("${simulator_output}")

if(simulator_status EQUAL 0)
    message(FATAL_ERROR "vacask unexpectedly succeeded")
endif()

if(NOT simulator_status MATCHES "^[0-9]+$")
    message(FATAL_ERROR "vacask terminated abnormally: ${simulator_status}")
endif()

if(NOT simulator_output MATCHES "${EXPECTED_ERROR}")
    message(FATAL_ERROR "Expected error did not appear: ${EXPECTED_ERROR}")
endif()
