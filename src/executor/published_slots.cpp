#include "executor/published_slots.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "proto/submission/published_slot_layout.hpp"

namespace av {

PublishedSlots::PublishedSlots(GuestBytes &bytes) : bytes_(bytes), offset_(0u), count_(0u)
{
    AV_SCOPE("PublishedSlots::PublishedSlots");
}

bool PublishedSlots::bind(uint64_t offset, uint32_t count)
{
    AV_SCOPE("PublishedSlots::bind");
    unbind();
    const PublishedSlotLayout tail(bytes_.length());
    if (!tail.keeps_word_clear(offset, count)) return false;
    if (bytes_.at(offset, uint64_t{count} * PublishedSlotLayout::slot_bytes) == nullptr) return false;
    offset_ = offset;
    count_ = count;
    return true;
}

void PublishedSlots::unbind()
{
    AV_SCOPE("PublishedSlots::unbind");
    offset_ = 0u;
    count_ = 0u;
}

bool PublishedSlots::bound() const
{
    AV_SCOPE("PublishedSlots::bound");
    return count_ != 0u;
}

/* The sequence is cleared before the payload and set after it, each behind a
 * store fence: a guest that reads its sequence before and after its copy saw
 * no write of this in between. */
bool PublishedSlots::write(uint32_t index, uint64_t sequence, const uint8_t *payload, uint32_t size)
{
    AV_SCOPE("PublishedSlots::write");
    if (count_ == 0u || size > PublishedSlotLayout::data_bytes || sequence == PublishedSlotLayout::unwritten)
        return false;
    uint8_t *const slot = bytes_.at(offset_ + uint64_t{index % count_} * PublishedSlotLayout::slot_bytes,
                                     PublishedSlotLayout::slot_bytes);
    if (slot == nullptr) return false;
    auto *const word = reinterpret_cast<uint64_t *>(slot + PublishedSlotLayout::sequence_at);
    __atomic_store_n(word, PublishedSlotLayout::unwritten, __ATOMIC_RELAXED);
    __builtin_ia32_sfence();
    if (size != 0u) __builtin_memcpy(slot + PublishedSlotLayout::data_at, payload, size);
    __builtin_memcpy(slot + PublishedSlotLayout::size_at, &size, sizeof size);
    __builtin_ia32_sfence();
    __atomic_store_n(word, sequence, __ATOMIC_RELEASE);
    __builtin_ia32_sfence();
    return true;
}

bool PublishedSlots::publish_error_word(uint64_t value)
{
    AV_SCOPE("PublishedSlots::publish_error_word");
    const PublishedSlotLayout tail(bytes_.length());
    if (!tail.holds_tail()) return false;
    uint8_t *const word = bytes_.at(tail.error_word(), PublishedSlotLayout::error_word_bytes);
    if (word == nullptr || reinterpret_cast<uintptr_t>(word) % sizeof(uint64_t) != 0u) return false;
    __atomic_store_n(reinterpret_cast<uint64_t *>(word), value, __ATOMIC_RELEASE);
    __builtin_ia32_sfence();
    return true;
}

bool PublishedSlots::invariants_hold() const
{
    AV_SCOPE("PublishedSlots::invariants_hold");
    return count_ != 0u || offset_ == 0u;
}

}  // namespace av
