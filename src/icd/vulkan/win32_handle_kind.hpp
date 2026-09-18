/* One external-object kind, and how its Win32 handle type is said to the host
 * (feature 047, US3, research.md §10).
 *
 * A Win32 handle here is this ICD's own, and the sharing behind it is the
 * host's fd: so a query about VK_EXTERNAL_*_HANDLE_TYPE_OPAQUE_WIN32_BIT is
 * asked of the host about its opaque fd, and the host's answer is said back in
 * Win32's spelling. Every other Win32 type - KMT, D3D11, D3D12 - is answered
 * as absent rather than translated, because nothing here turns a Direct3D
 * object into a host one (contracts/external_objects.md).
 *
 * Two instances, one per kind, because the memory and semaphore bits are
 * different flag types with the same arithmetic. */
#ifndef AV_ICD_WIN32_HANDLE_KIND_HPP
#define AV_ICD_WIN32_HANDLE_KIND_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class Win32HandleKind {
public:
    /* `win32_bit` is the opaque Win32 handle type this kind answers, `fd_bit`
     * the host's opaque fd type it is answered over. */
    Win32HandleKind(VkFlags win32_bit, VkFlags fd_bit);
    Win32HandleKind(const Win32HandleKind &) = delete;
    Win32HandleKind &operator=(const Win32HandleKind &) = delete;

    /* Whether `asked` is the one Win32 type this kind answers, and so a query
     * this ICD takes over rather than passes on. */
    bool    answers(VkFlags asked) const;
    /* The type the host is asked about in place of `asked`. */
    VkFlags host_type() const;
    /* The host's set of types, said in Win32's spelling: the Win32 bit where
     * the host named its fd, and nothing else - a host type the guest could
     * not name is not one to report it. */
    VkFlags reported(VkFlags host_types) const;

    /* The two bits it holds are each one bit, and not the same one. */
    bool invariants_hold() const;

private:
    VkFlags win32_bit_;
    VkFlags fd_bit_;
};

}  // namespace av::icd

#endif  // AV_ICD_WIN32_HANDLE_KIND_HPP
