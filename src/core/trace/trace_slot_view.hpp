#ifndef AV_TRACE_SLOT_VIEW_HPP
#define AV_TRACE_SLOT_VIEW_HPP

#include <stdint.h>

namespace av {

class TraceSlotView {
public:
    TraceSlotView();
    TraceSlotView(const uint8_t *slot, uint32_t frames);

    bool     used() const;
    bool     active() const;
    uint64_t thread_key() const;
    uint32_t thread_number() const;
    uint32_t depth() const;
    uint32_t stored() const;
    uint64_t entries() const;
    uint32_t last_left() const;
    uint64_t left_at() const;
    uint64_t waiting_on() const;
    uint64_t holding() const;
    uint32_t name(uint32_t frame) const;
    uint16_t level(uint32_t frame) const;
    uint64_t entered(uint32_t frame) const;
    uint64_t text(uint32_t frame) const;

private:
    uint64_t read(uint32_t offset, uint32_t bytes) const;
    uint64_t frame_field(uint32_t frame, uint32_t offset, uint32_t bytes) const;

    const uint8_t *slot_;
    uint32_t       frames_;
};

}

#endif
