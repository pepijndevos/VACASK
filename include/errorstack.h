#ifndef __ERRORSTACK_DEFINED
#define __ERRORSTACK_DEFINED

#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "status.h"
#include "common.h"


namespace NAMESPACE {

//
// Error registry
//
// Every error type is declared between ERRORCLASS() and END_ERRORCLASS(),
// which also registers it before main():
//
//     ERRORCLASS(MyError)
//         std::string format() const { return "..."; }
//     END_ERRORCLASS(MyError);
//
// For a fixed-message error with no members, use SIMPLE_ERRORCLASS(T, msg).
//
// Registration assigns the type a small integer code, records how to
// format/destroy/relocate it, and updates the running max size/alignment over
// all error types. An ErrorStack reads that max size/alignment when it is
// constructed, so every error type has to be registered before the first
// ErrorStack that will hold it is built.
//

// Type-erased operations on an error object stored in a slot.
using FormatterFn  = std::string(*)(const void* self);          // return its message
using DestructorFn = void(*)(void* self);                       // run ~T()
using RelocateFn   = void(*)(void* dst, void* src);             // move-construct dst from src, then ~src

// What the registry stores for each error type.
struct ErrorInfo {
    FormatterFn  formatter;
    DestructorFn destructor;
    RelocateFn   relocate;
    std::size_t  size;
    std::size_t  align;
};

// The registry and the running maxima over all registered error types.
inline std::vector<ErrorInfo>& error_registry() { static std::vector<ErrorInfo> r; return r; }
inline std::size_t& max_size()  { static std::size_t s = 0; return s; }
inline std::size_t& max_align() { static std::size_t a = 0; return a; }

// Base class of every error type. Carries the sentinel code: -1 until the type
// is registered, then its registry index.
template <typename T>
struct RegisteredError {
    static inline int code = -1;
};

// Register error type T. Idempotent: safe to call more than once.
template <typename T>
int register_error() {
    if (RegisteredError<T>::code >= 0) {
        return RegisteredError<T>::code;
    }

    int code = static_cast<int>(error_registry().size());
    error_registry().push_back(ErrorInfo{
        [](const void* self) -> std::string { return static_cast<const T*>(self)->format(); },
        [](void* self) { static_cast<T*>(self)->~T(); },
        [](void* dst, void* src) {
            T* s = static_cast<T*>(src);
            ::new (dst) T(std::move(*s));
            s->~T();
        },
        sizeof(T),
        alignof(T)
    });

    if (sizeof(T)  > max_size())  { max_size()  = sizeof(T); }
    if (alignof(T) > max_align()) { max_align() = alignof(T); }

    RegisteredError<T>::code = code;
    return code;
}

// Registry code of an error type (-1 if not registered).
template <typename T>
int code_of() { return RegisteredError<T>::code; }

// Registry entry for a code.
inline const ErrorInfo& info_of(int code) { return error_registry().at(code); }

// Open an error type body. It must provide `std::string format() const`.
// Members go straight after this line; close with END_ERRORCLASS(T).
#define ERRORCLASS(T) struct T : RegisteredError<T> {

// Register an error type before main().
#define REGISTER_ERRORCLASS(T) inline const int errreg_##T = register_error<T>()

// Close an ERRORCLASS(T) body and register the type.
#define END_ERRORCLASS(T) }; \
    REGISTER_ERRORCLASS(T);

#define SIMPLE_ERRORCLASS(T, msg) struct T : RegisteredError<T> { \
    std::string format() const { return (msg); } \
}; \
REGISTER_ERRORCLASS(T)

//
// Two examples
//

ERRORCLASS(GenericError)
    std::string format() const { return "Generic error."; }
END_ERRORCLASS(GenericError);

ERRORCLASS(GenericErrorWithValue)
    int value;
    GenericErrorWithValue(int value) : value(value) {}
    std::string format() const { return "Generic error (" + std::to_string(value) + ")."; }
END_ERRORCLASS(GenericErrorWithValue);


//
// Error stack
//

// A stack of error objects of arbitrary registered types, kept in one
// contiguous buffer. Every slot has the same footprint - max_size() rounded up
// to a multiple of max_align() - so the buffer only needs max_align() alignment
// for every slot to suit any error type. The buffer starts with room for 8
// errors and doubles when it fills up.
class ErrorStack {
public:
    // Slot geometry is taken from the registry as it stands now.
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

        int code = RegisteredError<U>::code;
        if (code < 0) {
            throw std::runtime_error("ErrorStack::push: error type not registered");
        }
        if (sizeof(U) > slotSize_ || alignof(U) > slotAlign_) {
            throw std::runtime_error("ErrorStack::push: error type registered after this stack was constructed");
        }

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

// Passed to core members instead of Status.
// Makes it possible to format and accumulate immediately in Status
// or collect in ErrorStack and format later. 
class ErrorConsumer {
public:
    ErrorConsumer() : status_(nullptr), errors_(nullptr), pushed(0) {};
    ErrorConsumer(Status& s) : status_(&s), errors_(nullptr), pushed(0) {};
    ErrorConsumer(ErrorStack& s) : status_(nullptr), errors_(&s), pushed(0) {};

    template<typename T>
    void push(T&& err) {
        if (errors_) {
            errors_->push(std::forward<T>(err));
        } else if (status_) {
            if (pushed==0) {
                status_->set(Status::Analysis, err.format());
            } else {
                status_->extend(err.format());
            }
        }
        pushed++;
    };

    void clear() {
        if (errors_) {
            errors_->clear();
        } else if (status_) {
            status_->clear();
        }
        pushed = 0;
    };

    // Concatenated messages of everything collected so far ("" for an sink that throws away messages).
    std::string format() const {
        if (errors_) {
            return errors_->format();
        } else if (status_) {
            return status_->message();
        }
        return {};
    };

    size_t pushedCount() const { return pushed; };
    
private:
    Status* status_{nullptr};
    ErrorStack* errors_{nullptr};
    size_t pushed;
};

}

#endif
