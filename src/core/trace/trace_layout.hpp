#ifndef AV_TRACE_LAYOUT_HPP
#define AV_TRACE_LAYOUT_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class ByteRange;

class TraceLayout {
public:
    enum : uint32_t {
        version = 1u,
        header_bytes = 96u,
        slot_header_bytes = 56u,
        frame_bytes = 24u,
        frames_per_slot = 32u,
        slot_bytes = slot_header_bytes + frames_per_slot * frame_bytes,
        module_bytes = 16u,
        max_slots = 256u,
    };
    enum : uint32_t {
        magic_at = 0u, version_at = 8u, header_bytes_at = 12u, slot_count_at = 16u, slot_bytes_at = 20u,
        frames_at = 24u, frame_bytes_at = 28u, module_at = 32u, process_at = 48u, crash_thread_at = 52u,
        created_at = 56u, ticks_at = 64u, crash_code_at = 72u, crash_address_at = 80u, crashed_at = 88u,
    };
    enum : uint32_t {
        owner_at = 0u, depth_at = 8u, thread_at = 12u, entries_at = 16u, last_left_at = 24u, left_at = 32u,
        waiting_at = 40u, holding_at = 48u,
    };
    enum : uint32_t { name_at = 0u, level_at = 4u, entered_at = 8u, text_at = 16u };
    static constexpr uint64_t magic = 0x0045434152545641ull;
    static constexpr uint64_t ticks_per_second = 10000000ull;
    static constexpr uint64_t active = 1ull << 63u;

    explicit TraceLayout(uint32_t slots);
    explicit TraceLayout(const ByteRange &region);

    uint32_t slots() const;
    size_t   bytes() const;
    size_t   slot(uint32_t index) const;
    size_t   frame(uint32_t index, uint32_t depth) const;

private:
    uint32_t slots_;
};

}

#endif
