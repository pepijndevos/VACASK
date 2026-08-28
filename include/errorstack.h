#ifndef __ERRORSTACK_DEFINED
#define __ERRORSTACK_DEFINED

#include <cstddef>
#include <new>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <unordered_map>
#include "common.h"


namespace NAMESPACE {

//
// Error registry
//

// Formatter, takes void pointer to error object, returns the message
using FormatterFn = std::string(*)(const void* self);

// Destructor, runs ~T() on the error object stored in a slot
using DestructorFn = void(*)(void* self);

// Relocator, move-constructs the error object at src into dst and destroys src.
// Used when the stack buffer is reallocated; std::string and friends are not
// trivially relocatable on every standard library, so a real move is needed.
using RelocateFn = void(*)(void* dst, void* src);

// TypeTag is a (void*) pointer to a TypeTag struct with member id=0,
// There is one TypeTag per registered error type.
using TypeTag = const void*;

// Struct to which TypeTag will point to, one per error type
template <typename T>
struct TypeTagOf { static const char id; };

template <typename T>
const char TypeTagOf<T>::id = 0;

// Return type tag (void*) for given error type
template <typename T>
TypeTag type_tag() { return &TypeTagOf<T>::id; }

// Information on an error type
struct ErrorInfo {
    FormatterFn formatter;
    DestructorFn destructor;
    RelocateFn relocate;
    std::size_t size;
    std::size_t align;
};

// Error formatter registry
inline std::vector<ErrorInfo>& error_registry() {
    static std::vector<ErrorInfo> r;
    return r;
}

// Max alignment over all registered error types
inline std::size_t& max_align() {
    static std::size_t a = 0;
    return a;
}

// Max size over all registered error types
inline std::size_t& max_size() {
    static std::size_t s = 0;
    return s;
}

// Register error type
template <typename T>
int register_error() {
    int code = static_cast<int>(error_registry().size());

    auto s = sizeof(T);
    auto a = alignof(T);
    error_registry().push_back(ErrorInfo{
        [](const void* self) -> std::string {
            return static_cast<const T*>(self)->format();
        },
        [](void* self) {
            static_cast<T*>(self)->~T();
        },
        [](void* dst, void* src) {
            T* s = static_cast<T*>(src);
            ::new (dst) T(std::move(*s));
            s->~T();
        },
        s,
        a
    });
    if (s > max_size()) {
        max_size() = s;
    }
    if (a > max_align()) {
        max_align() = a;
    }
    return code;
}

// Registered code for each error type
template <typename T>
inline int registered_code = register_error<T>();

// Get code of type
template <typename T>
int code_of() { return registered_code<T>; }

// Error info of an error type
inline const ErrorInfo& info_of(int code) {
    return error_registry().at(code);
}

// Base class that autoregisters an error type
template <typename T>
struct RegisteredError {
    static inline int code = registered_code<T>;  // forces registration eagerly
};

// Convenience macro for declaring an error type
#define ERRORCLASS(T) struct T : RegisteredError<T>

//
// Two examples
//

ERRORCLASS(GenericError) {
    std::string format() const {
        return "Genric error.";
    };
};

ERRORCLASS(GenericErrorWithCode) {
    int code;
    GenericErrorWithCode(int code) : code(code) {};
    std::string format() const {
        return "Genric error ("+std::to_string(code)+").";
    };
};

//
// Error stack
//

// Stores a stack of error objects of arbitrary registered types in a single
// contiguous buffer. Every slot has the same footprint: max_size() bytes rounded
// up to a multiple of max_align(), so the buffer only needs to be aligned to
// max_align() for every slot to be correctly aligned for any error type.
//
// The buffer starts with room for 8 errors and doubles whenever it fills up.
class ErrorStack {
public:
    // Slot geometry is fixed at construction from the registry. All error types
    // are registered eagerly at static-init time, so max_size()/max_align() are
    // already final by the time any stack is built.
    ErrorStack()
        : slotAlign_(max_align()),
          slotSize_((max_size() + max_align() - 1) / max_align() * max_align()) {}
    ~ErrorStack() { reset(); }

    // Non-copyable and non-movable: the buffer holds raw objects with
    // type-erased lifetime.
    ErrorStack           (const ErrorStack&)  = delete;
    ErrorStack           (      ErrorStack&&) = delete;
    ErrorStack& operator=(const ErrorStack&)  = delete;
    ErrorStack& operator=(      ErrorStack&&) = delete;

    // Move an error object onto the stack.
    template<typename T>
    void push(T&& err) {
        using U = std::remove_reference_t<T>;
        static_assert(!std::is_lvalue_reference_v<T>, "push an rvalue error onto the stack");
        int code = code_of<U>();
        void* dst = acquireSlot();
        ::new (dst) U(std::move(err));
        codes_.push_back(code);
    }

    // Destroy all stored errors, keep the buffer for reuse.
    void clear() {
        for (std::size_t i = 0; i < codes_.size(); i++) {
            info_of(codes_[i]).destructor(slotAt(i));
        }
        codes_.clear();
    }

    // Destroy all stored errors and release the buffer.
    void reset() {
        clear();
        if (buffer_ != nullptr) {
            ::operator delete(buffer_, std::align_val_t{slotAlign_});
        }
        buffer_ = nullptr;
        capacity_ = 0;
    }

    // Concatenate the messages of entries [first, last), first entry on top,
    // one per line (newline between entries, none after the last).
    std::string format(std::size_t first, std::size_t last) const {
        std::string out;
        for (std::size_t i = first; i < last; i++) {
            if (i != first) {
                out += '\n';
            }
            out += info_of(codes_.at(i)).formatter(slotAt(i));
        }
        return out;
    }

    // Format the single message of entry i.
    std::string format(std::size_t i) const { return format(i, i + 1); }

    // Concatenate all messages, entry 0 first.
    std::string format() const { return format(0, codes_.size()); }

    // Number of errors currently on the stack.
    std::size_t size() const { return codes_.size(); }

    bool empty() const { return codes_.empty(); }

private:
    static constexpr std::size_t initialCapacity = 8;

    void* slotAt(std::size_t i) const {
        return static_cast<char*>(buffer_) + i * slotSize_;
    }

    // Return a pointer to the next free slot, growing the buffer if needed.
    void* acquireSlot() {
        if (buffer_ == nullptr) {
            grow(initialCapacity);
        } else if (codes_.size() == capacity_) {
            grow(capacity_ * 2);
        }
        return slotAt(codes_.size());
    }

    // Allocate a larger buffer and relocate existing errors into it.
    void grow(std::size_t newCapacity) {
        void* nb = ::operator new(newCapacity * slotSize_, std::align_val_t{slotAlign_});
        for (std::size_t i = 0; i < codes_.size(); i++) {
            void* dst = static_cast<char*>(nb) + i * slotSize_;
            info_of(codes_[i]).relocate(dst, slotAt(i));
        }
        if (buffer_ != nullptr) {
            ::operator delete(buffer_, std::align_val_t{slotAlign_});
        }
        buffer_ = nb;
        capacity_ = newCapacity;
    }

    const std::size_t slotAlign_;    // alignment of the buffer and every slot
    const std::size_t slotSize_;     // bytes per slot (max_size rounded to align)
    void* buffer_ = nullptr;         // contiguous slot buffer
    std::size_t capacity_ = 0;       // number of slots the buffer holds
    std::vector<int> codes_;         // registry code of each stored error
};

}

#endif
