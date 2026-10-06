set(python_files
    src/vacask/rawfile.py
    src/vacask/vacaskpp.py
    src/vacask/runtest.py
    src/vacask/sg13g2tovc.py
    src/vacask/xschem2vc.py
    src/vacask/sg13g2xschem.tcl
    src/vacask/ts2vc.py
    src/vacask/ng2vc.py
    src/vacask/ng2vclib/__init__.py 
    src/vacask/ng2vclib/converter.py 
    src/vacask/ng2vclib/dfl.py 
    src/vacask/ng2vclib/exc.py 
    src/vacask/ng2vclib/generators.py 
    src/vacask/ng2vclib/patterns.py
    src/vacask/ng2vclib/m_devices.py 
    src/vacask/ng2vclib/m_file.py 
    src/vacask/ng2vclib/m_inst_d.py 
    src/vacask/ng2vclib/m_inst_n.py 
    src/vacask/ng2vclib/m_inst_passive.py 
    src/vacask/ng2vclib/m_inst_q.py 
    src/vacask/ng2vclib/m_inst_x.py 
    src/vacask/ng2vclib/m_masters.py 
    src/vacask/ng2vclib/m_params.py
    src/vacask/ng2vclib/m_model.py 
    src/vacask/ng2vclib/m_output.py 
)

# Prepare directory for staging the osdi library
set(STAGED_PYTHON_DIRECTORY "${STAGED_LIBRARY_DIRECTORY}/python/src")
file(MAKE_DIRECTORY "${STAGED_PYTHON_DIRECTORY}")

# Copy .py files to staging directory
set(python_staged_files "")
foreach(item IN ITEMS ${python_files})
    # Get directory name
    get_filename_component(tmpdirectory "${item}" DIRECTORY)
    if (NOT "${tmpdirectory}" STREQUAL "")
        file(MAKE_DIRECTORY "${STAGED_PYTHON_DIRECTORY}/${tmpdirectory}")
    endif()
    add_custom_command(
        OUTPUT "${STAGED_PYTHON_DIRECTORY}/${item}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${item}"
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/${item}"
        COMMAND ${CMAKE_COMMAND} ARGS -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${item}" "${STAGED_PYTHON_DIRECTORY}/${tmpdirectory}"
        COMMENT "Staging Python file ${item}"
    )
    list(APPEND python_staged_files "${STAGED_PYTHON_DIRECTORY}/${item}")
endforeach()

add_custom_target(
    "stage_scripts" ALL 
    SOURCES "${python_files}"
    DEPENDS "${python_staged_files}"
)
