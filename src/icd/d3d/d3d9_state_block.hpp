/* One Direct3D 9 state block the driver holds for d3d9.dll (pfnStateSet):
 * the state recorded between BEGIN and END, each piece once with the last
 * value recorded, which EXECUTE applies to the device and CAPTURE refreshes
 * from it (docs/research/d3d9/reverse-engineering.md §4.4). The runtime keeps
 * the handle and later sends only EXECUTE, so a block the driver loses is
 * state the application set and never sees applied.
 *
 * Its entries grow in blocks from the process heap: a block recorded from
 * CreateStateBlock(D3DSBT_ALL) holds every render, sampler and stage state
 * and hundreds of shader constants. */
#ifndef AV_ICD_D3D9_STATE_BLOCK_HPP
#define AV_ICD_D3D9_STATE_BLOCK_HPP

#include "core/support/status.hpp"
#include "icd/d3d/d3d9_state_entry.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::d3d {

class D3d9StateShadow;

class D3d9StateBlock {
public:
    /* What one block may hold: everything a D3DSBT_ALL block names, twice over. */
    static constexpr uint32_t max_entries = 8192u;

    explicit D3d9StateBlock(BlockSource &memory);
    ~D3d9StateBlock();
    D3d9StateBlock(const D3d9StateBlock &) = delete;
    D3d9StateBlock &operator=(const D3d9StateBlock &) = delete;

    /* The entry, replacing one that names the same piece of state. no_memory
     * when there is no room for another, and the block is as it was. */
    [[nodiscard]] Status record(const D3d9StateEntry &entry);
    /* Each entry takes the value `shadow` has for it now; one the shadow
     * never saw set keeps the value it was recorded with. */
    void                 capture(const D3d9StateShadow &shadow);

    uint32_t              count() const;
    const D3d9StateEntry &at(uint32_t index) const;

    bool invariants_hold() const;

private:
    [[nodiscard]] bool grow();

    BlockSource    &memory_;
    D3d9StateEntry *entries_;
    uint32_t        count_;
    uint32_t        capacity_;
};

}  // namespace av::d3d

#endif  // AV_ICD_D3D9_STATE_BLOCK_HPP
