#include <fstream>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include "fileread.h"
#include "common.h"


namespace NAMESPACE {

// SI prefix scale factor, same prefixes as in the netlist lexer
static bool siPrefixScale(const char* p, Real& scale) {
    if (p[0]==0) {
        scale = 1.0;
        return true;
    }
    if (p[0]=='m' && p[1]=='e' && p[2]=='g' && p[3]==0) {
        scale = 1e6;
        return true;
    }
    if (p[0]=='m' && p[1]=='i' && p[2]=='l' && p[3]==0) {
        scale = 25.4e-6;
        return true;
    }
    if (p[1]!=0) {
        return false;
    }
    switch (p[0]) {
        case 'm': scale=1e-3; break;
        case 'u': scale=1e-6; break;
        case 'n': scale=1e-9; break;
        case 'p': scale=1e-12; break;
        case 'f': scale=1e-15; break;
        case 'a': scale=1e-18; break;
        case 'k': scale=1e3; break;
        case 'K': scale=1e3; break;
        case 'M': scale=1e6; break;
        case 'x': scale=1e6; break;
        case 'X': scale=1e6; break;
        case 'G': scale=1e9; break;
        case 'T': scale=1e12; break;
        default: return false;
    }
    return true;
}

bool readNumbers(const std::string& fileName, std::vector<Real>& values, Status& s) {
    std::ifstream in(fileName);
    if (!in) {
        s.set(Status::NotFound, std::string("Cannot open file '")+fileName+"'.");
        return false;
    }
    auto isSeparator = [](char c) {
        return c==' ' || c=='\t' || c=='\r' || c=='\v' || c=='\f' || c==',' || c==';';
    };
    std::string line;
    std::string token;
    size_t lineNumber = 0;
    while (std::getline(in, line)) {
        lineNumber++;
        // Skip comment lines
        auto first = line.find_first_not_of(" \t");
        if (first!=std::string::npos && line.compare(first, 2, "//")==0) {
            continue;
        }
        size_t pos = 0;
        while (pos<line.size()) {
            if (isSeparator(line[pos])) {
                pos++;
                continue;
            }
            auto start = pos;
            while (pos<line.size() && !isSeparator(line[pos])) {
                pos++;
            }
            token.assign(line, start, pos-start);
            // Mantissa must start with a digit or dot (rejects inf, nan)
            const char* p = token.c_str();
            const char* q = (*p=='+' || *p=='-') ? p+1 : p;
            bool ok = (*q>='0' && *q<='9') || *q=='.';
            Real value = 0.0;
            Real scale = 1.0;
            char* end = nullptr;
            if (ok) {
                errno = 0;
                value = std::strtod(p, &end);
                ok = end!=p && !(errno==ERANGE && std::isinf(value)) && siPrefixScale(end, scale);
            }
            if (!ok) {
                s.set(Status::Syntax, std::string("Bad number '")+token+"' in file '"+fileName+"', line "+std::to_string(lineNumber)+".");
                return false;
            }
            values.push_back(value*scale);
        }
    }
    if (in.bad()) {
        s.set(Status::NotFound, std::string("Error reading file '")+fileName+"'.");
        return false;
    }
    return true;
}

}
