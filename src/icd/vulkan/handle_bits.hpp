/* The 64 bits of a Vulkan handle, whichever shape the build gives the handle
 * type: a pointer in a 64-bit build; on x86 a NonDispatchableHandle, or a
 * 32-bit pointer for a dispatchable one. The ICD makes some handles from its
 * own slots' addresses and hands the host's through as others, and reads both
 * back; this is the one conversion either shape takes, where a cast would
 * compile for only one of them. */
#ifndef AV_HANDLE_BITS_HPP
#define AV_HANDLE_BITS_HPP

#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av::icd {

class HandleBits {
public:
    /* A handle, a pointer, or the 64-bit value itself. */
    template <typename H>
    explicit HandleBits(H handle);

    uint64_t value() const;
    /* The bits as a handle or pointer of type H. */
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

}  // namespace av::icd

#endif  // AV_HANDLE_BITS_HPP
