/* The avgpu adapters of this machine, as the vendor libraries describe them
 * (docs/vendor-libraries.md): read from Windows on each ask, every adapter
 * handle closed again. avgpu presents its host GPU's vendor, device and
 * revision as its own, so each adapter reads as that vendor's GPU; an
 * application that trusts only a GPU its vendor's library vouches for finds
 * it there, described as everything else in the machine describes it. */
#ifndef AV_VENDOR_VENDOR_ADAPTERS_HPP
#define AV_VENDOR_VENDOR_ADAPTERS_HPP

#include "core/support/status.hpp"
#include "icd/vendor/vendor_adapter.hpp"

#include <stdint.h>

namespace av {
class Kmt;
}  // namespace av

namespace av::vendor {

class VendorAdapters {
public:
    static constexpr uint32_t max_adapters = 16u;

    explicit VendorAdapters(Kmt &kmt);
    VendorAdapters(const VendorAdapters &) = delete;
    VendorAdapters &operator=(const VendorAdapters &) = delete;

    /* Every avgpu adapter Windows enumerates (subsystem 1af4:5256, whatever
     * vendor it presents), in enumeration order. The kernel's refusal to
     * enumerate is the answer, with nothing learnt; an adapter one of whose
     * facts Windows will not give is left out - nothing is guessed. */
    [[nodiscard]] Status learn();

    /* Of the adapters presenting `vendor` (a PCI vendor ID), how many, and
     * the `index`th in enumeration order - null past them. */
    uint32_t             count_of(uint32_t vendor) const;
    const VendorAdapter *nth_of(uint32_t vendor, uint32_t index) const;

private:
    /* The facts of `adapter`, whose identity is `identity`, in `out`; false
     * where one cannot be read. */
    bool describe(uint32_t adapter, const PciIdentity &identity, uint64_t primary_luid, VendorAdapter &out);

    Kmt          &kmt_;
    VendorAdapter adapters_[max_adapters];
    uint32_t      count_;
};

}  // namespace av::vendor

#endif  // AV_VENDOR_VENDOR_ADAPTERS_HPP
