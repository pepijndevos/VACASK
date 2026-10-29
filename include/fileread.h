#ifndef __FILEREAD_DEFINED
#define __FILEREAD_DEFINED

#include <string>
#include <vector>
#include "value.h"
#include "status.h"
#include "common.h"


namespace NAMESPACE {

// Read numbers from a file, ignore lines starting with optional whitespace followed by //
// Numbers are separated by whitespace, comma, or semicolon
// A number is a C integer or floating point literal with optional sign and SI prefix
// Numbers are appended to values
bool readNumbers(const std::string& fileName, std::vector<Real>& values, Status& s=Status::ignore);

}

#endif
