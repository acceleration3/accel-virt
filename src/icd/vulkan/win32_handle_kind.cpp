#include "icd/vulkan/win32_handle_kind.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

Win32HandleKind::Win32HandleKind(VkFlags win32_bit, VkFlags fd_bit) : win32_bit_(win32_bit), fd_bit_(fd_bit)
{
    AV_SCOPE("Win32HandleKind::Win32HandleKind");
}

bool Win32HandleKind::answers(VkFlags asked) const
{
    AV_SCOPE("Win32HandleKind::answers");
    return asked == win32_bit_;
}

VkFlags Win32HandleKind::host_type() const
{
    AV_SCOPE("Win32HandleKind::host_type");
    return fd_bit_;
}

VkFlags Win32HandleKind::reported(VkFlags host_types) const
{
    AV_SCOPE("Win32HandleKind::reported");
    return (host_types & fd_bit_) != 0u ? win32_bit_ : 0u;
}

bool Win32HandleKind::invariants_hold() const
{
    AV_SCOPE("Win32HandleKind::invariants_hold");
    const bool one_bit = win32_bit_ != 0u && (win32_bit_ & (win32_bit_ - 1u)) == 0u && fd_bit_ != 0u &&
                         (fd_bit_ & (fd_bit_ - 1u)) == 0u;
    return one_bit && win32_bit_ != fd_bit_;
}

}
