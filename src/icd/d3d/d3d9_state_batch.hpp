#ifndef AV_ICD_D3D9_STATE_BATCH_HPP
#define AV_ICD_D3D9_STATE_BATCH_HPP

#include <stdint.h>

namespace av::d3d {

class D3d9StateBatch {
public:
    static constexpr uint32_t render_states = 256u;
    static constexpr uint32_t samplers = 21u;
    static constexpr uint32_t sampler_states = 14u;
    static constexpr uint32_t stages = 8u;
    static constexpr uint32_t stage_states = 33u;

    D3d9StateBatch();

    void render_state(uint32_t state, uint32_t value);
    void stage_state(uint32_t stage, uint32_t state, uint32_t value);

    bool            pending() const;
    uint32_t        render_count() const;
    const uint32_t *render_values() const;
    uint32_t        sampler_count() const;
    const uint32_t *sampler_values() const;
    uint32_t        stage_count() const;
    const uint32_t *stage_values() const;
    void            taken();
    void            forget();

    bool invariants_hold() const;

private:
    struct Slot {
        uint32_t sent_;
        uint32_t pending_at_;
        bool     known_;
    };

    uint32_t sampler_slot(uint32_t sampler) const;
    void     sampler_state(uint32_t sampler, uint32_t state, uint32_t value);
    void     queue(Slot &slot, uint32_t *list, uint32_t &count, uint32_t width, uint32_t first, uint32_t second,
                   uint32_t value);

    Slot     render_slots_[render_states];
    Slot     sampler_slots_[samplers * sampler_states];
    Slot     stage_slots_[stages * stage_states];
    uint32_t render_[render_states * 2u];
    uint32_t render_count_;
    uint32_t sampler_[samplers * sampler_states * 3u];
    uint32_t sampler_count_;
    uint32_t stage_[stages * stage_states * 3u];
    uint32_t stage_count_;
};

}

#endif
