#ifndef AV_ICD_D3D9_STATE_SHADOW_HPP
#define AV_ICD_D3D9_STATE_SHADOW_HPP

#include "icd/d3d/d3d9_state_entry.hpp"

#include <stdint.h>

namespace av::d3d {

class D3d9StateShadow {
public:
    static constexpr uint32_t render_states = 256u;
    static constexpr uint32_t stages = 21u;
    static constexpr uint32_t stage_states = 35u;
    static constexpr uint32_t float_registers = 256u;
    static constexpr uint32_t integer_registers = 16u;
    static constexpr uint32_t boolean_registers = 16u;
    static constexpr uint32_t streams = 16u;
    static constexpr uint32_t clip_planes = 6u;

    D3d9StateShadow();

    bool set(const D3d9StateEntry &entry);
    bool current(const D3d9StateEntry &entry, D3d9StateEntry &value) const;

private:
    struct Slot {
        uint32_t value_[D3d9StateEntry::value_words];
        bool     known_;
    };

    Slot       *slot(const D3d9StateEntry &entry);
    const Slot *slot(const D3d9StateEntry &entry) const;
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

}

#endif
