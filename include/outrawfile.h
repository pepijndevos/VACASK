#ifndef __OUTRAWFILE_DEFINED
#define __OUTRAWFILE_DEFINED

#include "value.h"
#include "identifier.h"
#include "circuit.h"
#include "output.h"
#include "errorstack.h"
#include "flags.h"
#include <complex>
#include <vector>
#include <string>
#include <iostream>
#include "common.h"


namespace NAMESPACE {

enum class RawfileFlags {
    None = 0, 
    Binary = 1,  // ASCII otherwise
    Complex = 2, // Real otherwise
    Padded = 4,  // Unpadded otherwise
};
DEFINE_FLAG_OPERATORS(RawfileFlags);

ERRORCLASS(OutputRawfileWriteFailed)
    std::string fileName;
    OutputRawfileWriteFailed(std::string fileName) : fileName(std::move(fileName)) {}
    std::string format() const { return "Failed to write file '" + fileName + "'."; }
END_ERRORCLASS(OutputRawfileWriteFailed);

class OutputRawfile : public Output, public FlagBase<RawfileFlags> {
public:
    OutputRawfile(const std::string& baseName, Output::SourcesList& sources, Flags f=Flags::Binary|Flags::Padded);
    ~OutputRawfile();

    bool prologue(ErrorConsumer& errors) override;
    bool addPoint(ErrorConsumer& errors) override;
    bool epilogue(ErrorConsumer& errors) override;
    bool remove(Status& s=Status::ignore) override;

private:
    std::streampos pointCountPos;
    size_t count;
    std::string fileName;
};

}

#endif
