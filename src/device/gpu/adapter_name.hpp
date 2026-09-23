/* The name the guest shows for the adapter a host GPU becomes - in Device
 * Manager, WMI and DXGI - carried in the capset's name field
 * (docs/protocol.md).
 *
 * It is the Vulkan deviceName without a trailing parenthesised group, which
 * Mesa's drivers append to name themselves and the chip rather than the
 * product: "AMD Radeon RX 9070 XT (RADV GFX1201)" becomes "AMD Radeon RX 9070
 * XT", "llvmpipe (LLVM 19.1.7, 256 bits)" becomes "llvmpipe". The deviceName
 * is chosen over the PCI database's name because pci.ids names the silicon
 * ("Navi 48 [Radeon RX 9070/9070 XT/9070 GRE]"), not the product the vendor's
 * own Windows driver would show. A name that is nothing but a parenthesised
 * group is kept whole. */
#ifndef AV_ADAPTER_NAME_HPP
#define AV_ADAPTER_NAME_HPP

#include "proto/context/gpu_identity.hpp"

#include <stddef.h>

namespace av {

class AdapterName {
public:
    /* `vulkan_name` is NUL-terminated. The result is cut to fit the capset's
     * field at a UTF-8 character boundary, never inside a character. */
    explicit AdapterName(const char *vulkan_name);

    const char *text() const;

private:
    static constexpr size_t capacity = GpuIdentity::name_bytes - 1u;

    /* The length of `name` once a trailing " (...)" is dropped: `length` when
     * there is none, or when nothing would be left. */
    size_t without_suffix(const char *name, size_t length) const;
    /* `length` shortened to at most capacity, never splitting a character. */
    size_t fitted(const char *name, size_t length) const;

    char text_[GpuIdentity::name_bytes];
};

}  // namespace av

#endif  // AV_ADAPTER_NAME_HPP
