#ifndef AV_ICD_WIN32_HANDLE_KIND_HPP
#define AV_ICD_WIN32_HANDLE_KIND_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class Win32HandleKind {
public:
    Win32HandleKind(VkFlags win32_bit, VkFlags fd_bit);
    Win32HandleKind(const Win32HandleKind &) = delete;
    Win32HandleKind &operator=(const Win32HandleKind &) = delete;

    bool    answers(VkFlags asked) const;
    VkFlags host_type() const;
    VkFlags reported(VkFlags host_types) const;

    bool invariants_hold() const;

private:
    VkFlags win32_bit_;
    VkFlags fd_bit_;
};

}

#endif
