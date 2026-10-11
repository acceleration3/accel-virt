/* What avgpu.sys answers a user-mode driver's UMDRIVERPRIVATE query with:
 * which adapter the runtime opened, and what the device under it offers the
 * user-mode driver beyond the capset. A user-mode driver from before the
 * features asks for the first two words and gets them; one that asks for all
 * three from a miniport from before gets two, and reads no feature. */
#ifndef AV_UMD_ADAPTER_INFO_HPP
#define AV_UMD_ADAPTER_INFO_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class UmdAdapterInfo {
public:
    static constexpr size_t   base_size = 16u;
    static constexpr size_t   encoded_size = 24u;
    /* A blt-model present may name a present snapshot instead of pages the
     * user-mode driver read back (docs/protocol.md, "Present engine"). */
    static constexpr uint64_t feature_present_snapshots = 1u;
    /* The host GPU doing the work is integrated - it shares the host CPU's
     * memory (GpuIdentity::integrated_gpu): what a vendor's management
     * library says of the GPU it describes (src/icd/vendor). */
    static constexpr uint64_t feature_integrated_gpu = 2u;

    UmdAdapterInfo();
    UmdAdapterInfo(uint64_t luid, uint64_t features);

    uint64_t luid() const;
    bool     offers(uint64_t feature) const;

    /* All three words where `capacity` holds them, the first two where only
     * those fit, the count in `written`: too_large for less than two. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity, size_t &written) const;
    /* malformed for less than two words or another tag; a third word is read
     * where there is one. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint64_t tag = 0x0044494C555641ull;   /* "AVULID" */

    uint64_t luid_;
    uint64_t features_;
};

}  // namespace av

#endif  // AV_UMD_ADAPTER_INFO_HPP
