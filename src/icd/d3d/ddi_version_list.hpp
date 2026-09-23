/* The DDI version words one runtime is served at, and the count-then-fill
 * answer its GetSupportedVersions expects (contracts/umd_interface.md).
 *
 * A word is (interface << 32) | (build << 16), the way d3d10umddi.h composes
 * D3DWDDM3_2_DDI_SUPPORTED. A version this driver has not implemented and
 * tested is not in the list (constitution VI), which is why there is one word
 * per runtime today and why adding one is a feature's decision, not a
 * constant's. */
#ifndef AV_D3D_DDI_VERSION_LIST_HPP
#define AV_D3D_DDI_VERSION_LIST_HPP

#include "icd/d3d/hresult.hpp"
#include "icd/d3d/runtime_kind.hpp"

#include <stdint.h>

namespace av::d3d {

struct DdiVersionListProbe;

class DdiVersionList {
public:
    /* Room for the revisions 056, 057 and 064 will add without reshaping this. */
    static constexpr uint32_t max_words = 8u;

    explicit DdiVersionList(RuntimeKind runtime);
    DdiVersionList(const DdiVersionList &) = delete;
    DdiVersionList &operator=(const DdiVersionList &) = delete;

    uint32_t count() const;
    /* 0 past the count: a runtime asking for a word this driver does not hold
     * is asking about a version it was never told about. */
    uint64_t word(uint32_t index) const;
    /* Whether an Interface an OpenAdapter or a CreateDevice asked with is the
     * interface half of a word in this list. */
    bool     serves(uint32_t interface_version) const;
    /* Whether an OpenAdapter may ask with this Interface: the runtime opens
     * with the oldest interface of the family it will negotiate in (d3d11.dll
     * on 26100 asks with WDDM 2.0's) and settles the device's version through
     * GetSupportedVersions, so any interface of a served word's major at or
     * below that word opens. */
    bool     opens(uint32_t interface_version) const;
    /* A null `entries` is refused. A null `words` answers the count. An
     * `*entries` below the count is refused with the count written back, as
     * the runtime's own buffer growth expects. */
    Hresult  answer(uint32_t *entries, uint64_t *words) const;

    bool invariants_hold() const;

private:
    uint64_t words_[max_words];
    uint32_t count_;

    friend struct DdiVersionListProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_DDI_VERSION_LIST_HPP
