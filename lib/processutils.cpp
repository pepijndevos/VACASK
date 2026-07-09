#ifdef SIMMSC
#define _WIN32_WINNT 0x0601
#endif

#include <boost/version.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/read.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/environment.hpp>
#include <boost/process/v2/stdio.hpp>
#include <boost/dll.hpp>

#include <exception>
#include <filesystem>
#include <map>
#include <string>
#include "processutils.h"
#include "filesystem.h"
#include "libplatform.h"
#include "simulator.h"
#include "status.h"

namespace bp2 = boost::process::v2;

namespace NAMESPACE {

bool findProgram(const std::string& prog, std::string& pathToProg) {
    static std::unordered_map<std::string, std::string> cache;

    // Try cache
    auto it = cache.find(prog);
    if (it!=cache.end()) {
        pathToProg = it->second;
        return true;
    } else {
        // All calls to findFile() 
        // - return original (not canonical) name 
        // - do not check file size
        if (std::filesystem::path(prog).is_absolute()) {
            // Absolute path, check if the program exists
            if (findFile(prog, pathToProg, false, false)) {
                cache.insert({prog, pathToProg});
                return true;
            }
        } else if (findFile((std::filesystem::path(executableFile()).parent_path() / prog).string(), pathToProg, false, false)) {
            // Not absolute path, look in the directory of the simulator executable file
            cache.insert({prog, pathToProg});
            return true;
        } else if (auto [ok, sysPathToProg] = findFileInSystemPath(prog, false, false); ok) {
            // Finally, look in the system path 
            pathToProg = std::move(sysPathToProg);
            cache.insert({prog, pathToProg});
            return true;
        } 
    }
    return false;
}

// TODO: replace this with something better
std::tuple<bool, std::string, std::string> runProcess(
    const std::string& prog, const std::vector<std::string>& args, 
    const std::string* pythonPath, const std::string* parentFilePath, 
    bool collect, bool debugFiles, Status& s
) {
    
    bool ok;
    std::string path;

    if (!findProgram(prog, path)) {
        s.set(Status::NotFound, "Executable '"+prog+"' not found.");
        return std::make_tuple(false, "", "");
    }

    if (debugFiles) {
        Simulator::dbg() << "Running: '" << path;
        for(auto& arg : args) {
            Simulator::dbg() << " \"" << arg << "\"";
        }
        Simulator::dbg() << "'.\n";
    }

    // Make a copy of the environment in form of a map of key-value string pairs
    std::map<std::string, std::string> customEnv;
    for (const auto& kv : bp2::environment::current()) {
        if (kv.key().empty()) {
            continue;
        }
        auto key = kv.key().string();
        // Handle empty env string bug
        auto value = kv.value().empty() ? std::string(" ") : kv.value().string();
        customEnv.emplace(key, value);
    }
    
    // Add pythonPath to PATHONPATH environmental variable
    if (pythonPath) {
        customEnv["PYTHONPATH"] += pathSeparator()+(*pythonPath);
    }
    
    // Add source file parent directory to environment under SIM_PARENT_FILE_DIR
    if (parentFilePath) {
        customEnv["SIM_PARENT_FILE_DIR"] = *parentFilePath;
    }

    if (collect) {
        boost::asio::io_context ios;
        
        boost::asio::readable_pipe rp_out{ios};
        boost::asio::readable_pipe rp_err{ios};

        std::string out_str;
        std::string err_str;

        bool out_error = false;
        bool err_error = false;

        bp2::process c(
            ios.get_executor(), 
            path, 
            args, 
            bp2::process_environment(customEnv),
            bp2::process_stdio{
                .out = rp_out, 
                .err = rp_err
            }
        );

        boost::asio::async_read(
            rp_out, boost::asio::dynamic_buffer(out_str), boost::asio::transfer_all(), 
            [&](const boost::system::error_code& ec, std::size_t n) {
                if (ec.value()==boost::system::errc::success || ec.value() == boost::asio::error::eof) {
                } else {
                    // We land here with a broken pipe error, no worried though, the process finished
                    out_error = true;
                }
            }
        );
        boost::asio::async_read(
            rp_err, boost::asio::dynamic_buffer(err_str), boost::asio::transfer_all(), 
            [&](const boost::system::error_code& ec, std::size_t n) {
                if (ec.value()==boost::system::errc::success || ec.value() == boost::asio::error::eof) {
                } else {
                    // We land here with a broken pipe error, no worried though, the process finished
                    err_error = true;
                }
            }
        );

        ios.run();
        c.wait();

        auto ec = c.exit_code();
        bool ok = ec==0;
        if (!ok) {
            s.set(Status::Process, "Error running '"+prog+"', exit code="+std::to_string(c.exit_code())+".");
        }

        return std::make_tuple(ok, out_str, err_str); 
    } else {
        boost::asio::io_context ios;
        bp2::process c(
            ios.get_executor(), 
            path, 
            args, 
            bp2::process_environment(customEnv)
        );
        
        c.wait(); // wait for the process to exit   
        
        bool ok = c.exit_code()==0;
        if (!ok) {
            s.set(Status::Process, "Error running '"+prog+"', exit code="+std::to_string(c.exit_code())+".");
        }

        return std::make_tuple(ok, "", "");
    }
}

std::string executableFile() {
    static auto str = boost::dll::program_location().string();
    return str;
}

}
