#include "proto/submission/published_slot_layout.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

PublishedSlotLayout::PublishedSlotLayout(uint64_t arena_bytes) : arena_bytes_(arena_bytes)
{
    AV_SCOPE("PublishedSlotLayout::PublishedSlotLayout");
}

bool PublishedSlotLayout::holds_tail() const
{
    AV_SCOPE("PublishedSlotLayout::holds_tail");
    return arena_bytes_ >= error_word_bytes && arena_bytes_ % error_word_bytes == 0u;
}

uint64_t PublishedSlotLayout::error_word() const
{
    AV_SCOPE("PublishedSlotLayout::error_word");
    return holds_tail() ? arena_bytes_ - error_word_bytes : 0u;
}

uint64_t PublishedSlotLayout::slots_at(uint32_t count) const
{
    AV_SCOPE("PublishedSlotLayout::slots_at");
    const uint64_t span = uint64_t{count} * slot_bytes;
    const uint64_t word = error_word();
    if (count == 0u || word < span + slot_bytes) return 0u;
    return (word - span) / slot_bytes * slot_bytes;
}

bool PublishedSlotLayout::keeps_word_clear(uint64_t offset, uint32_t count) const
{
    AV_SCOPE("PublishedSlotLayout::keeps_word_clear");
    const uint64_t span = uint64_t{count} * slot_bytes;
    const uint64_t word = error_word();
    return count != 0u && offset % slot_bytes == 0u && offset <= word && span <= word - offset;
}

}
