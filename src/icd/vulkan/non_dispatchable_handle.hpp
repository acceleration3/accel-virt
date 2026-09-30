/* A Vulkan non-dispatchable handle as the 32-bit guest build defines it.
 *
 * On x86 vk_platform.h's VK_USE_64_BIT_PTR_DEFINES is 0, so every
 * non-dispatchable handle - VkBuffer, VkImage, VkSemaphore - is the same
 * uint64_t. The generated client overloads on the handle's type, as do the
 * ICD's handle tables, and on x86 those overloads would all be one function.
 * vulkan_core.h lets a build define VK_DEFINE_NON_DISPATCHABLE_HANDLE itself,
 * and this is what it is defined as: a distinct type per handle, as the pointer
 * types of the 64-bit build are, holding the 64-bit value.
 *
 * The ABI is the integer's. The class is trivially copyable, eight bytes and
 * aligned as a uint64_t, so it is passed on the stack, and laid out inside a
 * Vulkan structure, exactly as the uint64_t an application or the loader has.
 * No Vulkan command returns a handle by value, which is the one place the MSVC
 * x86 ABI treats a class unlike its integer.
 *
 * Force-included into the x86 guest build's C++ ahead of the Khronos headers
 * (src/um/CMakeLists.txt); the 64-bit builds never see it. */
#ifndef AV_NON_DISPATCHABLE_HANDLE_HPP
#define AV_NON_DISPATCHABLE_HANDLE_HPP

#ifdef __cplusplus

#include <stdint.h>

namespace av {

template <typename Tag>
class NonDispatchableHandle {
public:
    constexpr NonDispatchableHandle() = default;
    /* VK_NULL_HANDLE, which this build defines as nullptr. */
    constexpr NonDispatchableHandle(decltype(nullptr));
    constexpr explicit NonDispatchableHandle(uint64_t bits);

    constexpr explicit operator uint64_t() const;
    constexpr explicit operator bool() const;
    constexpr bool operator==(const NonDispatchableHandle &) const = default;
    constexpr bool operator==(decltype(nullptr)) const;

private:
    uint64_t bits_ = 0u;
};

template <typename Tag>
constexpr NonDispatchableHandle<Tag>::NonDispatchableHandle(decltype(nullptr)) : bits_(0u)
{
}

template <typename Tag>
constexpr NonDispatchableHandle<Tag>::NonDispatchableHandle(uint64_t bits) : bits_(bits)
{
}

template <typename Tag>
constexpr NonDispatchableHandle<Tag>::operator uint64_t() const
{
    return bits_;
}

template <typename Tag>
constexpr NonDispatchableHandle<Tag>::operator bool() const
{
    return bits_ != 0u;
}

template <typename Tag>
constexpr bool NonDispatchableHandle<Tag>::operator==(decltype(nullptr)) const
{
    return bits_ == 0u;
}

}  // namespace av

#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef ::av::NonDispatchableHandle<struct object##_T> object;
#define VK_NULL_HANDLE nullptr

#endif  // __cplusplus

#endif  // AV_NON_DISPATCHABLE_HANDLE_HPP
