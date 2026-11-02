#ifndef AV_HANDLE_BITS_HPP
#define AV_HANDLE_BITS_HPP

#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av::icd {

class HandleBits {
public:
    template <typename H>
    explicit HandleBits(H handle);

    uint64_t value() const;
    template <typename H>
    H as() const;

private:
    uint64_t bits_;
};

template <typename H>
HandleBits::HandleBits(H handle) : bits_(0u)
{
    AV_SCOPE("HandleBits::HandleBits");
    static_assert(sizeof(H) <= sizeof(bits_));
    __builtin_memcpy(&bits_, &handle, sizeof(H));
}

template <typename H>
H HandleBits::as() const
{
    AV_SCOPE("HandleBits::as");
    static_assert(sizeof(H) <= sizeof(bits_));
    H handle{};
    __builtin_memcpy(&handle, &bits_, sizeof(H));
    return handle;
}

}

#endif
