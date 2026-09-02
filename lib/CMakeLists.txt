FLEX_TARGET(dfllexer 
    dfllexer.l "${CMAKE_CURRENT_BINARY_DIR}/dfllexer.cpp" 
    COMPILE_FLAGS "-d"
)

BISON_TARGET(dflparser 
    dflparser.y "${CMAKE_CURRENT_BINARY_DIR}/dflparser.cpp" 
    DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/dflparser.h"
    COMPILE_FLAGS "-Wcounterexamples" 
)

ADD_FLEX_BISON_DEPENDENCY(dfllexer dflparser)

add_library(simlib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/common.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/generator.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/acct.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/libplatform.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/flags.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/exportdef.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/dynload.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/limitfunctions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/status.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/value.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/parameterized.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/elsetup.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/devbase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/devbuiltin.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/devvisrc.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/devctlsrc.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/devbuiltins.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/hierdevice.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/osdi.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/osdicallback.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/osdifile.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/osdidevice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/osdimodel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/osdiinstance.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/parseroutput.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/location.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/dflscanner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/processutils.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/filesystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/filestack.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/sourceloc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/pool.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/rpnexpr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/rpnexprva.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/identifier.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/rpnfunctor.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/rpnbuiltin.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/context.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/rpnstack.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/rpnevalctx.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/rpneval.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/asa241.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/options.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/comdata.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/natures.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/introspection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/circuit.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coredelay.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/openvafcomp.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/cscmatrix.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/cscblkmatrix.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/cscblkmatrix.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/solver.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/solklu.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/simulator.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/output.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/outrawfile.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/densematrix.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/blaslapack.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/nrsolver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/homotopy.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/hmtpgmin.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/hmtpsrc.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/progress.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/spurs.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/core.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coreop.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coreopnr.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coredcinc.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coredcxf.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coreac.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coreacxf.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coreacstb.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coreacsp.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/corenoise.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coretrancoef.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coretrannr.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coretran.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/corehb.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/corehbnr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/an.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/ansupport.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/ansolution.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/coresweep.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/anop.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/andcinc.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/andcxf.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/anac.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/anacxf.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/anacstb.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/anacsp.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/annoise.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/antran.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/anhb.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/corepsstran.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/corepss.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/anpss.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/tdnblock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/tdnzohwhite.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/tdnzohflicker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/tdnsdewhite.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/tdnsdeflicker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/anhb.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/anhbac.h
    acct.cpp
    libplatform.cpp 
    dynload.cpp 
    limitfunctions.cpp
    status.cpp
    value.cpp
    parameterized.cpp
    devbase.cpp
    devvisrc.cpp 
    devctlsrc.cpp 
    devbuiltins.cpp 
    hierdevice.cpp
    osdicallback.cpp
    osdifile.cpp
    osdifilenature.cpp
    osdidevice.cpp
    osdimodel.cpp
    osdiinstance.cpp 
    parseroutput.cpp
    processutils.cpp
    filesystem.cpp
    filestack.cpp
    sourceloc.cpp
    pool.cpp
    identifier.cpp
    rpnexpr.cpp
    rpnexprva.cpp
    rpnbuiltin.cpp
    context.cpp
    rpnstack.cpp
    rpnevalctx.cpp
    rpneval.cpp
    asa241.cpp
    introspection.cpp
    options.cpp 
    comdata.cpp
    natures.cpp
    node.cpp
    circuit.cpp
    coredelay.cpp
    openvafcomp.cpp
    ciroptions.cpp 
    cirvariables.cpp 
    cirparams.cpp
    cscmatrix.cpp
    cscblkmatrix.cpp
    simulator.cpp
    output.cpp
    outrawfile.cpp
    densematrix.cpp
    nrsolver.cpp 
    nrforces.cpp 
    homotopy.cpp 
    hmtpgmin.cpp
    hmtpsrc.cpp
    progress.cpp
    spurs.cpp
    core.cpp 
    coreop.cpp 
    coreopnr.cpp 
    coredcinc.cpp 
    coredcxf.cpp 
    coreac.cpp 
    coreacxf.cpp 
    coreacstb.cpp 
    coreacsp.cpp 
    corenoise.cpp
    coretrancoef.cpp 
    coretrannr.cpp 
    coretran.cpp
    corehb.cpp
    corehbcoloc.cpp
    corehbxform.cpp
    corehbnr.cpp
    corehbac.cpp
    an.cpp
    ansupport.cpp 
    ansolution.cpp 
    coresweep.cpp
    anop.cpp
    andcinc.cpp 
    andcxf.cpp 
    anac.cpp 
    anacxf.cpp 
    anacstb.cpp 
    anacsp.cpp 
    annoise.cpp 
    antran.cpp
    anhb.cpp
    tdnblock.cpp
    tdnzohwhite.cpp
    tdnzohflicker.cpp
    tdnsdewhite.cpp
    tdnsdeflicker.cpp
    anhbac.cpp
    corepsstran.cpp
    corepss.cpp
    anpss.cpp
    location.cpp
    "${BISON_dflparser_OUTPUTS}"
    "${FLEX_dfllexer_OUTPUTS}"
    parser.cpp
    
)
target_include_directories(simlib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${FLEX_INCLUDE_DIRS}
)
# target_compile_definitions(dlib PRIVATE BUILDING_SIM_DLL)

# OpenMP is scoped to just these TUs (see the top-level CMakeLists.txt); the rest
# of simlib is built without -fopenmp. libplatform.cpp needs it for
# omp_get_max_threads() / omp_set_num_threads() (guarded by OPENMP_ENABLED).
if (OpenMP_CXX_FOUND)
    set_source_files_properties(libplatform.cpp PROPERTIES
        COMPILE_OPTIONS "${OpenMP_CXX_FLAGS}"
        COMPILE_DEFINITIONS "OPENMP_ENABLED"
    )
endif()

# SuperLU_MT backend: added only when CMakeLists.txt found it (SIM_HAVE_SUPERLU).
# Its headers clash with blaslapack.h, so the SuperLU include dir, the -fopenmp
# flag and -D__OPENMP (the library's threading structs depend on it) are scoped
# to these two TUs and must not reach any other.
if (SIM_HAVE_SUPERLU)
    target_sources(simlib PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/solsuperlu.h
        solsuperlu_common.h
        solsuperlu.cpp 
        solsuperlu_real.cpp
        solsuperlu_complex.cpp
    )
    set_source_files_properties(solsuperlu_real.cpp solsuperlu_complex.cpp PROPERTIES
        INCLUDE_DIRECTORIES "${SIM_SUPERLU_INCLUDE}"
        COMPILE_OPTIONS "${OpenMP_CXX_FLAGS}"
        COMPILE_DEFINITIONS "__OPENMP"
    )
endif()
