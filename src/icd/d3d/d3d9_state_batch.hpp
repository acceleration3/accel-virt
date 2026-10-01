/* A Direct3D 9 device's render, sampler and texture-stage states between
 * draws, sent as at most three commands before the draw that needs them.
 *
 * Source-engine games set dozens of states per draw, most of them to the
 * value they already have; each sent alone would be a command, and the
 * guest's per-command cost is what heavy scenes are limited by. So a state is
 * kept until taken (SetRenderStates9, SetSamplerStates9,
 * SetTextureStageStates9 carry what is pending), and a value the host already
 * has is not sent at all.
 *
 * The DDI has no sampler-state slot: sampler states arrive through
 * SetTextureStageState numbered as Direct3D 8 numbered them (D3DDDITSS_*), and
 * are split here into the API's sampler and stage states. States the API has
 * no place for - texture handles, colour keys, the DDI-only scene capture -
 * go nowhere. */
#ifndef AV_ICD_D3D9_STATE_BATCH_HPP
#define AV_ICD_D3D9_STATE_BATCH_HPP

#include <stdint.h>

namespace av::d3d {

class D3d9StateBatch {
public:
    static constexpr uint32_t render_states = 256u;
    /* 16 pixel samplers, then D3DDMAPSAMPLER and the four vertex samplers. */
    static constexpr uint32_t samplers = 21u;
    static constexpr uint32_t sampler_states = 14u;
    static constexpr uint32_t stages = 8u;
    static constexpr uint32_t stage_states = 33u;

    D3d9StateBatch();

    void render_state(uint32_t state, uint32_t value);
    /* `state` is a D3DDDITSS_*; `stage` 0-7, or a sampler number up to 260
     * for a sampler state. */
    void stage_state(uint32_t stage, uint32_t state, uint32_t value);

    bool            pending() const;
    /* What is pending, as the three commands carry it: (state, value) pairs,
     * and (sampler or stage, state, value) triples. */
    uint32_t        render_count() const;
    const uint32_t *render_values() const;
    uint32_t        sampler_count() const;
    const uint32_t *sampler_values() const;
    uint32_t        stage_count() const;
    const uint32_t *stage_values() const;
    /* What is pending has been sent: the host has it now. */
    void            taken();
    /* The host has nothing any more - a new device - so every value is sent
     * again, whatever it was. */
    void            forget();

    bool invariants_hold() const;

private:
    /* One state's place: what the host was last sent, whether it was sent at
     * all, and where it waits in its pending list (0 for nowhere, else the
     * entry's index + 1). */
    struct Slot {
        uint32_t sent_;
        uint32_t pending_at_;
        bool     known_;
    };

    /* A sampler number (0-15, 256-260) as an index into the sampler slots;
     * samplers for none. */
    uint32_t sampler_slot(uint32_t sampler) const;
    void     sampler_state(uint32_t sampler, uint32_t state, uint32_t value);
    /* `value` for `slot`, into `list` of `width`-word entries whose last word
     * is the value and whose leading words are `key`: in place if pending,
     * appended otherwise, nothing where the host has it already. */
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

}  // namespace av::d3d

#endif  // AV_ICD_D3D9_STATE_BATCH_HPP
