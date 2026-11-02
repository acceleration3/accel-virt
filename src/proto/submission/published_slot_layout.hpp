#ifndef AV_PUBLISHED_SLOT_LAYOUT_HPP
#define AV_PUBLISHED_SLOT_LAYOUT_HPP

#include <stdint.h>

namespace av {

class PublishedSlotLayout {
public:
    static constexpr uint64_t error_word_bytes = sizeof(uint64_t);
    static constexpr uint32_t slot_bytes = 128u;
    static constexpr uint32_t sequence_at = 0u;
    static constexpr uint32_t size_at = 8u;
    static constexpr uint32_t data_at = 16u;
    static constexpr uint32_t data_bytes = slot_bytes - data_at;
    static constexpr uint64_t unwritten = 0u;

    explicit PublishedSlotLayout(uint64_t arena_bytes);

    bool     holds_tail() const;
    uint64_t error_word() const;
    uint64_t slots_at(uint32_t count) const;
    bool     keeps_word_clear(uint64_t offset, uint32_t count) const;

private:
    uint64_t arena_bytes_;
};

}

#endif
