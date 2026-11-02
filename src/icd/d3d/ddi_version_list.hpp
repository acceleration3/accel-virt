#ifndef AV_D3D_DDI_VERSION_LIST_HPP
#define AV_D3D_DDI_VERSION_LIST_HPP

#include "icd/d3d/hresult.hpp"
#include "icd/d3d/runtime_kind.hpp"

#include <stdint.h>

namespace av::d3d {

struct DdiVersionListProbe;

class DdiVersionList {
public:
    static constexpr uint32_t max_words = 8u;

    explicit DdiVersionList(RuntimeKind runtime);
    DdiVersionList(const DdiVersionList &) = delete;
    DdiVersionList &operator=(const DdiVersionList &) = delete;

    uint32_t count() const;
    uint64_t word(uint32_t index) const;
    bool     serves(uint32_t interface_version) const;
    bool     opens(uint32_t interface_version) const;
    Hresult  answer(uint32_t *entries, uint64_t *words) const;

    bool invariants_hold() const;

private:
    uint64_t words_[max_words];
    uint32_t count_;

    friend struct DdiVersionListProbe;
};

}

#endif
