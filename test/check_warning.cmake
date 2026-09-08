execute_process(
    COMMAND "${SIMULATOR}" -dp "${NETLIST}"
    RESULT_VARIABLE simulator_status
    OUTPUT_VARIABLE simulator_stdout
    ERROR_VARIABLE simulator_stderr
)

set(simulator_output "${simulator_stdout}\n${simulator_stderr}")
message("${simulator_output}")

if(NOT simulator_status EQUAL 0)
    message(FATAL_ERROR "vacask exited with status ${simulator_status}")
endif()

if(NOT simulator_output MATCHES "${EXPECTED_WARNING}")
    message(FATAL_ERROR "Expected warning did not appear: ${EXPECTED_WARNING}")
endif()
