#include "outrawfile.h"
#include "libplatform.h"
#include <format>
#include <iomanip>
#include <filesystem>
#include "common.h"


namespace NAMESPACE {

// std::endl is slow because it flushes the stream, use \n

OutputRawfile::OutputRawfile(const std::string& baseName, Output::SourcesList& sources, Flags f) 
    : Output(baseName, sources), FlagBase(f), fileName(baseName+".raw") {
    // Delete old file before opening
    if (std::filesystem::exists(fileName)) {
        std::filesystem::remove(fileName);
    }
}

OutputRawfile::~OutputRawfile() {
    outStream.close();
}

bool OutputRawfile::prologue(ErrorConsumer& errors) {
    outStream.open(fileName, std::ios::binary | std::ios::out | std::ios::trunc);
    outStream << "Title: " << title_ << "\n";
    if (outStream.fail()) {
        errors.push(OutputRawfileWriteFailed{fileName});
        return false;
    }
    
    // Thread-safe local timestamp; asctime/localtime use shared static buffers.
    // floor<seconds> drops sub-second digits so %S matches asctime's layout.
    outStream << "Date: " << formattedTimestamp() << "\n";
    
    outStream << "Plotname: " << plotname_ << "\n";
    outStream << "Flags: " << (checkFlags(Flags::Complex) ? "complex" : "real" ) 
              << (checkFlags(Flags::Padded) ? "" : " unpadded") << "\n";
    outStream << "No. Variables: " << std::to_string(srcs.size()) << "\n";
    
    outStream << "No. Points: "; 
    pointCountPos = outStream.tellp();
    outStream << "                \n"; // 16 spaces for point count

    // Add "Dimensions: n,n,n,...\n" for plots with multidimensional vectors

    outStream << "Variables:\n";
    for(size_t i=0; i<srcs.size(); i++) {
        // outStream << "\t" << std::to_string(i) << "\t" << std::string(descrs[i].name) << "\t" << "notype"; 
        outStream << "\t" << std::to_string(i) << "\t" << std::string(srcs[i].name()) << "\t" << "notype"; 
        // TODO: add " dims=n,n,n,...\n" for rawfiles with vectors of different length
        outStream << "\n"; 
    }

    if (checkFlags(Flags::Binary)) {
        outStream << "Binary:\n";
    } else {
        outStream << "Values:\n";
        outStream << std::scientific << std::setprecision(15);
    }

    count=0;

    if (outStream.fail()) {
        errors.push(OutputRawfileWriteFailed{fileName});
        return false;
    }

    return true;
}

bool OutputRawfile::addPoint(ErrorConsumer& errors) {
    if (checkFlags(Flags::Binary)) {
        if (checkFlags(Flags::Complex)) {
            for(size_t i=0; i<srcs.size(); i++) {
                Complex c = srcs[i].getC();
                outStream.write(reinterpret_cast<char*>(&c), sizeof(Complex));
            }
        } else {
            for(size_t i=0; i<srcs.size(); i++) {
                double r = srcs[i].getR();
                outStream.write(reinterpret_cast<char*>(&r), sizeof(double));
            }
        }
    } else {
        if (checkFlags(Flags::Complex)) {
            for(size_t i=0; i<srcs.size(); i++) {
                if (i==0) {
                    outStream << " " << count;
                }
                Complex c = srcs[i].getC();
                outStream << "\t" << c.real() << "," << c.imag() << "\n";
            }
        } else {
            for(size_t i=0; i<srcs.size(); i++) {
                if (i==0) {
                    outStream << " " << count;
                }
                double r = srcs[i].getR();
                outStream << "\t" << r << "\n";
            }
        }
    }
    if (outStream.fail()) {
        errors.push(OutputRawfileWriteFailed{fileName});
        return false;
    }
    count++;

    return true;
}

bool OutputRawfile::epilogue(ErrorConsumer& errors) {
    outStream.seekp(pointCountPos);
    outStream << count;
    outStream.seekp(0, std::ios_base::end);
    // ASCII plots are separated by a blank line (binary plots are not).
    if (!checkFlags(Flags::Binary)) {
        outStream << "\n";
    }

    // close() flushes the buffer, small files are only written here
    outStream.close();
    if (outStream.fail()) {
        errors.push(OutputRawfileWriteFailed{fileName});
        return false;
    }

    return true;
}

bool OutputRawfile::remove(Status& s) {
    if (std::filesystem::exists(fileName)) {
        std::filesystem::remove(fileName);
    }

    return true;
}

}
