#include "parser.h"
#include "status.h"
#include "openvafcomp.h"
#include "circuit.h"
#include "simulator.h"
#include "processutils.h"
#include "platform.h"
#include "cmd.h"
#include "config.h"
#include "common.h"
#include <filesystem>
#include <unordered_set>
#include <stdexcept>

using namespace sim;

char helpText[] = 
    /* Usage: programName */ "[options] [<filename>]\n"
    "\nOptions:\n"
    "  -h, --help          print help and exit\n"
    "  -dp, --dump-paths   print location of simulator's components\n"
    "  -df, --debug-files  turn on debug output for file operations\n"
    "  -dt, --dump-tables  dump parser tables\n"
    "  -se, --skip-embed   do not dump embedded files\n"
    "  -sp, --skip-postprocess\n"
    "                      do not run postprocessing steps\n"
    "       --extra-tomlfile <file>\n"
    "                      addittionally load specified toml file as last\n"  
    "       --tomlfile <file>\n"
    "                      skip loading all toml files, except this one\n"  
    // "  -qw, --quiet-warnings\n"
    // "                      turn off warning messages\n"
    "  -qp, --quiet-progress\n"
    "                      turn off progress messages\n"
    "       --no-output    suppress output of result files\n"
#ifdef OPENMP_ENABLED
    "  -n <n>, --ncpu <n>  number of CPUs to use (1)\n"
    "                      <=0 .. autodetect, take OMP_NUM_THREADS into account\n"
    "                      or use all available CPUs (default)\n"
    "  -b <n>, --blas-ncpu <n>\n"
    "                      number of CPUs to assign to OpenBLAS (1)\n"
    "                      <=0 .. use OpenBLAS autodetect\n"
#endif
    ; 

int main(int argc, char**argv) {
    bool dumpPaths = false;
    bool dumpTables = false;
    bool fileDebug = false;
    bool dumpEmbed = true;
    bool runPostprocess = true;
    bool progress = true;
    bool noOutput = false;
    std::string extraTomlFile;
    std::string tomlFile;

    // <=0 means autodetect by OpenMP
    int ncpu = Simulator::defaultNcpu;

    // <=0 means autodetect
    int blasNcpu = Simulator::defaultBlasNcpu;

    // Simulator information
    Simulator::out() << 
        "This is "+Platform::programName+" "+Platform::programVersion+".\n"+Platform::programCopyright+"\n";
    Simulator::out() << 
        Platform::programHomepage+"\n";
    #ifdef OPENMP_ENABLED
    Simulator::out() << "OpenMP parallel processing\n";
    #endif
    #ifdef SIM_HAVE_SUPERLU
    Simulator::out() << "SuperLU_MT parallel linear solver\n";
    #endif
    #ifdef SIMDEBUG
    Simulator::out() << "\n" << "Warning! This is a debug build. Simulator will be slow.\n";
    #endif
    #ifdef SIMPROFILE
    Simulator::out() << "\n" << "Warning! This binary is instrumeted for profiling and code coverage.\n";
    #endif
    Simulator::out() << "\n";
    
    if (argc<2) {
        // No arguments, print a hint on help
        Simulator::err() << "To get help, run as: " << Platform::programName << " -h\n"; 
        return 1;
    } 
        
    // Parse an integer CLI argument; on failure print a message and return false.
    auto parseIntArg = [](const char* s, const char* what, int& out) -> bool {
        try {
            std::string str(s);
            size_t pos = 0;
            int v = std::stoi(str, &pos);
            if (pos != str.size()) throw std::invalid_argument("trailing chars");
            out = v;
            return true;
        } catch (const std::exception&) {
            Simulator::err() << "Invalid " << what << " '" << s << "'.\n";
            return false;
        }
    };

    // Parse arguments
    char* fileArg = nullptr;
    for(int i=1; i<argc; i++) {
        if (argv[i][0]!='-') {
            // Not an option
            if (i==argc-1) {
                // Last argument is file name, OK
                fileArg = argv[i];
                break;
            } else {
                // Not last argument, error
                Simulator::err() << "Unrecognized argument '" << argv[i] << "'.\n";
                return 1;
            }
        }
        // Must be an option
        std::string arg = argv[i];
        if (arg=="-h" || arg=="--help") {
            Simulator::out() << "Usage: " << Platform::programName << " " << helpText;
            return 0;
        } else if (arg=="-dp" || arg=="--dump-paths") {
            dumpPaths = true;
        } else if (arg=="-df" || arg=="--debug-files") {
            fileDebug = true;
        } else if (arg=="-dt" || arg=="--dump-tables") {
            dumpTables = true;
        } else if (arg=="-se" || arg=="--skip-embed") {
            dumpEmbed = false;
        } else if (arg=="-sp" || arg=="--skip-postprocess") {
            runPostprocess = false;
        } else if (arg=="-qp" || arg=="--quiet-progress") {
            progress = false;
        } else if (arg=="--no-output") {
            noOutput = true;
        } else if (arg=="--extra-tomlfile") {
            if (i+1>=argc) {
                Simulator::err() << "Missing extra TOML file name.\n";
                return 1;
            }
            i++;
            extraTomlFile = argv[i];
        } else if (arg=="--tomlfile") {
            if (i+1>=argc) {
                Simulator::err() << "Missing TOML file name.\n";
                return 1;
            }
            i++;
            tomlFile = argv[i];
        } else if (arg=="-n" || arg=="--ncpu") {
            if (i+1>=argc) {
                Simulator::err() << "Missing number of CPUs.\n";
                return 1;
            }
            i++;
            if (!parseIntArg(argv[i], "number of CPUs", ncpu)) {
                return 1;
            }
        } else if (arg=="-b" || arg=="--blas-ncpu") {
            if (i+1>=argc) {
                Simulator::err() << "Missing number of OpenBLAS CPUs.\n";
                return 1;
            }
            i++;
            if (!parseIntArg(argv[i], "number of OpenBLAS CPUs", blasNcpu)) {
                return 1;
            }
        } else {
            Simulator::err() << "Unrecognized argument '"+arg+"'.\n";
            return 1;
        }
    }

    // FileDebug and noOutput simulator flags
    Simulator::setFileDebug(fileDebug); 
    Simulator::setNoOutput(noOutput); 

    //Status
    Status status;

    // Setup platform defaults
    Platform::setup();

    // SIM_OPENVAF environmental variable
    auto openvafCstring = std::getenv("SIM_OPENVAF"); 
    if (openvafCstring) {
        Platform::setOpenVaf(openvafCstring);
    }

    // Get path to simulator binary
    auto simulatorBinary = executableFile();

    // Default module directory and include directory
    std::string defaultModuleDirectory = (Platform::libraryPath() / "mod").string();
    std::string defaultIncludeDirectory = (Platform::libraryPath() / "inc").string(); 

    // Overrides via SIM_MODULES_PATH and SIM_SOURCES_PATH environmental variables
    auto modCstring = std::getenv("SIM_MODULE_PATH"); 
    auto incCstring = std::getenv("SIM_INCLUDE_PATH"); 
    // Defaults when env var is not defined
    auto modStr = modCstring ? std::string(modCstring) : defaultModuleDirectory;
    auto incStr = incCstring ? std::string(incCstring) : defaultIncludeDirectory;

    // Setup simulator
    if (!Simulator::setup(modStr, incStr, ncpu, blasNcpu, status)) {
        Simulator::err() << status.message() << "\n";
        return 1;
    }

    // Load file passed as argument
    ParserTables tab;
    
    // Candidate config files
    std::vector<std::string> configFiles = {
        Platform::systemConfig(), 
        Platform::userConfig(), 
        Platform::localConfig()
    };

    // Extra TOML file
    if (extraTomlFile.size()>0) {
        configFiles.push_back(std::move(extraTomlFile));
    }

    // Override all TOML files
    if (tomlFile.size()>0) {
        configFiles.clear();
        configFiles.push_back(std::move(tomlFile));
    }
    
    // Add input file to file stack
    FileStackFileIndex stackPosition;
    if (fileArg) {
        // Add input file to filestack. Search only in local directory 
        // if fileArg is not an absolute path. Stop if file not found. 
        stackPosition = tab.fileStack().addFile(fileArg);
        if (stackPosition==FileStack::badFileId) {
            Simulator::err() << std::string("File '")+fileArg+"' not found.\n";
            return 1;
        }
        
        // Get canonical name
        auto canonical = tab.fileStack().canonicalName(stackPosition);

        // Get input file directory
        auto inputFileDir = std::filesystem::path(canonical).parent_path();
        
        // Add another cadidate config file
        configFiles.push_back((inputFileDir / ".vacaskrc.toml").string());
    }

    // Read configuration files, make sure you read each one only once
    std::unordered_set<std::string> processed_configs;
    for(auto& cfg : configFiles) {
        // Was it processed already, skip if yes
        if (processed_configs.contains(cfg)) {
            continue;
        }
        processed_configs.insert(cfg);
        // Open file
        std::ifstream f(cfg);
        if (f) {
            // File found, read it
            if (fileDebug) {
                Simulator::dbg() << "Reading config file: " << cfg << "\n";
            }
            if (!readConfig(f, cfg, status)) {
                Simulator::err() << status.message() << "\n";
                return 1;
            }
        }
    }

    // CPU info
    Simulator::dbg() << "Available CPUs:     " << cpuCount() << "\n";
    Simulator::dbg() << "CPUs used:          " << Simulator::nCpu() << "\n";
    Simulator::dbg() << "OpenBLAS CPUs:      " << blasCpuCount() << "\n";
    Simulator::dbg() << "\n";

    // Dump paths
    if (dumpPaths) {
        Simulator::dbg() << "Simulator binary:   " << simulatorBinary << "\n";
        Simulator::dbg() << "Startup directory:  " << Simulator::startupPath() << "\n";
        Simulator::dbg() << "Module path:\n";
        for(auto& d : Simulator::modulePath()) {
            Simulator::dbg() << "  " << d << "\n";
        }
        Simulator::dbg() << "Include path:\n";
        for(auto& d : Simulator::includePath()) {
            Simulator::dbg() << "  " << d << "\n";
        }
        std::string openVafPath;
        if (findProgram(Platform::openVaf(), openVafPath)) {
            Simulator::dbg() << "OpenVAF-Reloaded compiler: " << openVafPath << "\n";
            if (Platform::openVafArgs().size()>0) {
                Simulator::dbg() << "OpenVAF-Reloaded arguments: ";
                for(auto& arg : Platform::openVafArgs()) {
                    Simulator::dbg() << arg << " ";
                }
                
                Simulator::dbg() << "\n";
            }
        } else {
            Simulator::dbg() << "OpenVAF-Reloaded compiler not found.\n";
        }
        if (Platform::pythonExecutable().size()>0) {
            Simulator::dbg() << "Python interpreter: " << Platform::pythonExecutable() << "\n";
        }
        if (Platform::pythonPath().size()>0) {
            Simulator::dbg() << "Python path addition: " << Platform::pythonPath() << "\n";
        }
        Simulator::dbg() << "\n";
    }

    // Stop if no input file
    if (!fileArg) {
        Simulator::out() << "No input file specified.\n";
        return 0;
    }

    // Parser 
    Parser parser(tab);

    if (!parser.parseNetlistFile(stackPosition, status)) {
        Simulator::err() << status.message() << "\n";
        return 1;
    }

    if (!tab.verify(status)) {
        Simulator::err() << status.message() << "\n";
        return 1;
    }

    if (progress) {
        Simulator::dbg() << "Simulating: " << tab.title() << "\n";
    }

    // Process behavioral sources
    if (!tab.processBehaviorals(Simulator::fileDebug(), status)) {
        Simulator::err() << status.message() << "\n";
        return 1;
    }

    if (dumpTables) {
        Simulator::dbg() << "---- Parser tables ----\n";
        tab.dump(0, Simulator::dbg());
        Simulator::dbg() << "---- Parser tables end ----\n\n";
    }

    // Dump embedded files
    if (dumpEmbed) {
        auto ok = tab.writeEmbedded(Simulator::fileDebug(), status);
        if (!ok) {
            Simulator::err() << status.message() << "\n";
            return 1;
        }
    }
    
    // Create circuit
    OpenvafCompiler comp(Platform::openVaf(), Platform::openVafArgs());
    Circuit cirObj(tab, &comp, status);
    if (!cirObj.isValid()) {
        Simulator::err() << status.message() << "\n";
        return 1;
    }
    
    // Command interpreter
    CommandInterpreter interp(tab, tab.control(), cirObj);
    interp.setPrintProgress(progress);
    interp.setRunPostprocess(runPostprocess);
    
    // Run iterpreter
    if (interp.run(0, status)!=InterpreterExitStatus::EndReached) {
        Simulator::err() << status.message() << "\n";
        return 1;
    }
    
    
    return 0;
}
