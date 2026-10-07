#include "dynload.h"
#include "common.h"

#ifdef SIMWINDOWS
#include <windows.h> 
#else
#include <dlfcn.h>
#endif

namespace NAMESPACE {
#ifdef SIMWINDOWS
void* openDynamicLibrary(const char* fileName) {
    return LoadLibrary(fileName);
}

bool closeDynamicLibrary(void* handle) {
    if (FreeLibrary((HMODULE)handle)) {
        return true;
    } else {
        return false;
    }
}

void* dynamicLibrarySymbol(void* handle, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress((HMODULE)handle, name));
}

std::string dynamicLibraryError() {
    DWORD errCode = GetLastError();

    LPVOID lpMsgBuf = nullptr;
    // The A variant keeps the buffer char regardless of UNICODE
    DWORD retval = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&lpMsgBuf, 0, nullptr
    );

    std::string errMsg;
    if (retval == 0) {
        errMsg = std::string("Failed to format error message with code ")+std::to_string(errCode)+".";
    } else {
        errMsg = std::string(static_cast<char*>(lpMsgBuf));
    }
    if (lpMsgBuf) {
        LocalFree(lpMsgBuf);
    }

    return errMsg;
  }
#else
void* openDynamicLibrary(const char* fileName) {
    return dlopen(fileName, RTLD_NOW);
}

bool closeDynamicLibrary(void* handle) {
    if (dlclose(handle)==0) {
        return true;
    } else {
        return false;
    }
}

void* dynamicLibrarySymbol(void* handle, const char* name) {
    return dlsym(handle, name);
}

std::string dynamicLibraryError() {
    auto err = dlerror();
    if (err) {
        return err;
    } else {
        return "";
    }
}
#endif

}
