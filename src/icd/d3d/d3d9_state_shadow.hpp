/* A Direct3D 9 device's current value of every piece of state a state block
 * can hold, as the DDI last set it. d3d9.dll asks the driver to CAPTURE a
 * state block - refresh each of its entries from the device as it is now -
 * and the host cannot be asked mid-batch, so the guest keeps its own copy.
 * Kept in arrays indexed by the state, since setting is on every draw's path
 * and capturing is rare. */
#ifndef AV_ICD_D3D9_STATE_SHADOW_HPP
#define AV_ICD_D3D9_STATE_SHADOW_HPP

#include "icd/d3d/d3d9_state_entry.hpp"

#include <stdint.h>

namespace av::d3d {

class D3d9StateShadow {
public:
    static constexpr uint32_t render_states = 256u;
    /* Stage numbers 0-15, then 256-260 folded to 16-20. */
    static constexpr uint32_t stages = 21u;
    static constexpr uint32_t stage_states = 35u;
    static constexpr uint32_t float_registers = 256u;
    static constexpr uint32_t integer_registers = 16u;
    static constexpr uint32_t boolean_registers = 16u;
    static constexpr uint32_t streams = 16u;
    static constexpr uint32_t clip_planes = 6u;

    D3d9StateShadow();

    /* The entry's value is the device's now. False for one out of range,
     * which is kept nowhere. */
    bool set(const D3d9StateEntry &entry);
    /* `entry` with the device's value now; false where nothing was set. */
    bool current(const D3d9StateEntry &entry, D3d9StateEntry &value) const;

private:
    struct Slot {
        uint32_t value_[D3d9StateEntry::value_words];
        bool     known_;
    };

    /* Where the shadow keeps `entry`, or null for none. */
    Slot       *slot(const D3d9StateEntry &entry);
    const Slot *slot(const D3d9StateEntry &entry) const;
    /* A stage or sampler number as an index, stages for none. */
    uint32_t    stage_index(uint32_t stage) const;

    Slot render_[render_states];
    Slot stage_[stages * stage_states];
    Slot texture_[stages];
    Slot vertex_shader_;
    Slot pixel_shader_;
    Slot declaration_;
    Slot constant_f_[2u * float_registers];
    Slot constant_i_[2u * integer_registers];
    Slot constant_b_[2u * boolean_registers];
    Slot stream_[streams];
    Slot indices_;
    Slot viewport_;
    Slot z_range_;
    Slot scissor_;
    Slot clip_plane_[clip_planes];
};

}  // namespace av::d3d

#endif  // AV_ICD_D3D9_STATE_SHADOW_HPP
