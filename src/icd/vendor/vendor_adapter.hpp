/* One avgpu adapter as a GPU vendor's management library describes it - AMD's
 * Display Library, NVIDIA's NVAPI (docs/vendor-libraries.md). Every fact is
 * one Windows already reports for the adapter, so the library and Device
 * Manager, SetupAPI and DXGI agree about the one device. */
#ifndef AV_VENDOR_VENDOR_ADAPTER_HPP
#define AV_VENDOR_VENDOR_ADAPTER_HPP

#include "guest/kmt/pci_identity.hpp"
#include "icd/common/adapter_string.hpp"
#include "guest/kmt/pci_location.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class TextBuffer;
}  // namespace av

namespace av::vendor {

class VendorAdapter {
public:
    /* The name in UTF-8, ended within this (icd::AdapterString). */
    static constexpr size_t name_bytes = icd::AdapterString::bytes;

    VendorAdapter();
    /* `driver_version` is four 16-bit parts, the most significant first. */
    VendorAdapter(uint64_t luid, const PciIdentity &identity, const PciLocation &location, uint64_t driver_version,
                  uint64_t dedicated_video, uint64_t dedicated_system, uint64_t shared_system, bool integrated,
                  bool primary);

    /* The name as the adapter's driver wrote it (HardwareInformation.
     * AdapterString), UTF-16 and ended: what Device Manager and DXGI call
     * it. An unpaired surrogate becomes U+FFFD. */
    void take_name(const uint16_t *wide);

    uint64_t           luid() const;
    const PciIdentity &identity() const;
    const PciLocation &location() const;
    const char        *name() const;
    uint64_t           driver_version() const;
    /* The driver package's version as Windows writes it, "0.35.5.0". */
    void               append_driver_version(TextBuffer &text) const;
    uint64_t           dedicated_video_bytes() const;
    uint64_t           dedicated_system_bytes() const;
    uint64_t           shared_system_bytes() const;
    /* The host GPU doing the work shares the host CPU's memory. */
    bool               integrated() const;
    /* The primary display is on this adapter. */
    bool               primary() const;

private:
    uint64_t    luid_;
    PciIdentity identity_;
    PciLocation location_;
    icd::AdapterString name_;
    uint64_t    driver_version_;
    uint64_t    dedicated_video_;
    uint64_t    dedicated_system_;
    uint64_t    shared_system_;
    bool        integrated_;
    bool        primary_;
};

}  // namespace av::vendor

#endif  // AV_VENDOR_VENDOR_ADAPTER_HPP
