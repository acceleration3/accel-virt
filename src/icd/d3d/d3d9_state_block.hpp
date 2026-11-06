#ifndef AV_ICD_D3D9_STATE_BLOCK_HPP
#define AV_ICD_D3D9_STATE_BLOCK_HPP

#include "core/support/status.hpp"
#include "icd/d3d/d3d9_state_entry.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::d3d {

class D3d9StateShadow;

class D3d9StateBlock {
public:
    explicit D3d9StateBlock(BlockSource &memory);
    ~D3d9StateBlock();
    D3d9StateBlock(const D3d9StateBlock &) = delete;
    D3d9StateBlock &operator=(const D3d9StateBlock &) = delete;

    [[nodiscard]] Status record(const D3d9StateEntry &entry);
    void                 capture(const D3d9StateShadow &shadow);

    uint32_t              count() const;
    const D3d9StateEntry &at(uint32_t index) const;

    bool invariants_hold() const;

private:
    friend struct D3d9StateBlockProbe;

    [[nodiscard]] bool grow();

    BlockSource    &memory_;
    D3d9StateEntry *entries_;
    uint32_t        count_;
    uint32_t        capacity_;
};

}

#endif
