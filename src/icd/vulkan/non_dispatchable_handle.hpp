#ifndef AV_NON_DISPATCHABLE_HANDLE_HPP
#define AV_NON_DISPATCHABLE_HANDLE_HPP

#ifdef __cplusplus

#include <stdint.h>

namespace av {

template <typename Tag>
class NonDispatchableHandle {
public:
    constexpr NonDispatchableHandle() = default;
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

}

#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef ::av::NonDispatchableHandle<struct object##_T> object;
#define VK_NULL_HANDLE nullptr

#endif

#endif
