set(python_files
    vacask/__init__.py
    vacask/helpers.py
    vacask/rawfile.py
    vacask/vacaskpp.py
    vacask/runtest.py
    vacask/sg13g2tovc.py
    vacask/sg13cmos5ltovc.py
    vacask/gf180tovc.py
    vacask/xschem2vc.py
    vacask/sg13g2xschem.tcl
    vacask/sg13cmos5lxschem.tcl
    vacask/ts2vc.py
    vacask/ng2vc.py
    vacask/ng2vclib/__init__.py 
    vacask/ng2vclib/converter.py 
    vacask/ng2vclib/dfl.py 
    vacask/ng2vclib/exc.py 
    vacask/ng2vclib/generators.py 
    vacask/ng2vclib/patterns.py
    vacask/ng2vclib/m_devices.py 
    vacask/ng2vclib/m_file.py 
    vacask/ng2vclib/m_inst_d.py 
    vacask/ng2vclib/m_inst_n.py 
    vacask/ng2vclib/m_inst_passive.py 
    vacask/ng2vclib/m_inst_q.py 
    vacask/ng2vclib/m_inst_m.py
    vacask/ng2vclib/m_inst_x.py
    vacask/ng2vclib/m_masters.py 
    vacask/ng2vclib/m_params.py
    vacask/ng2vclib/m_model.py 
    vacask/ng2vclib/m_output.py 
)

# Prepare directory for staging the osdi library
set(STAGED_PYTHON_DIRECTORY "${STAGED_LIBRARY_DIRECTORY}/python")
file(MAKE_DIRECTORY "${STAGED_PYTHON_DIRECTORY}")

# Copy .py files to staging directory
set(python_staged_sources "")
set(python_staged_files "")
foreach(item IN ITEMS ${python_files})
    # Get directory name
    get_filename_component(tmpdirectory "${item}" DIRECTORY)
    if (NOT "${tmpdirectory}" STREQUAL "")
        file(MAKE_DIRECTORY "${STAGED_PYTHON_DIRECTORY}/${tmpdirectory}")
    endif()
    add_custom_command(
        OUTPUT "${STAGED_PYTHON_DIRECTORY}/${item}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/${item}"
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/src/${item}"
        COMMAND ${CMAKE_COMMAND} ARGS -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/${item}" "${STAGED_PYTHON_DIRECTORY}/${tmpdirectory}"
        COMMENT "Staging Python file ${item}"
    )
    list(APPEND python_staged_sources "src/${item}")
    list(APPEND python_staged_files "${STAGED_PYTHON_DIRECTORY}/${item}")
endforeach()

add_custom_target(
    "stage_scripts" ALL 
    SOURCES "${python_staged_sources}"
    DEPENDS "${python_staged_files}"
)
