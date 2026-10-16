#include "proto/submission/published_slot_layout.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

PublishedSlotLayout::PublishedSlotLayout(uint64_t arena_bytes) : arena_bytes_(arena_bytes)
{
    AV_SCOPE("PublishedSlotLayout::PublishedSlotLayout");
}

/* An arena whose size is not a whole number of words would put the word
 * where an aligned load cannot read it. */
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

}  // namespace av
