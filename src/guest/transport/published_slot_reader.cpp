#include "guest/transport/published_slot_reader.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/submission/published_slot_layout.hpp"

namespace av {

PublishedSlotReader::PublishedSlotReader() : slots_(nullptr), count_(0u)
{
    AV_SCOPE("PublishedSlotReader::PublishedSlotReader");
}

void PublishedSlotReader::take(uint8_t *slots, uint32_t count)
{
    AV_SCOPE("PublishedSlotReader::take");
    slots_ = count == 0u ? nullptr : slots;
    count_ = slots_ == nullptr ? 0u : count;
    volatile uint8_t *const cleared = slots_;
    for (uint64_t at = 0u; at < uint64_t{count_} * PublishedSlotLayout::slot_bytes; at++) cleared[at] = 0u;
}

bool PublishedSlotReader::reading() const
{
    AV_SCOPE("PublishedSlotReader::reading");
    return count_ != 0u;
}

bool PublishedSlotReader::read(uint32_t index, uint64_t sequence, void *payload, uint32_t size) const
{
    AV_SCOPE("PublishedSlotReader::read");
    if (count_ == 0u || sequence == PublishedSlotLayout::unwritten || size > PublishedSlotLayout::data_bytes)
        return false;
    const uint8_t *const slot = slots_ + uint64_t{index % count_} * PublishedSlotLayout::slot_bytes;
    const auto *const    word = reinterpret_cast<const uint64_t *>(slot + PublishedSlotLayout::sequence_at);
    if (__atomic_load_n(word, __ATOMIC_ACQUIRE) != sequence) return false;
    if (payload != nullptr && size != 0u) __builtin_memcpy(payload, slot + PublishedSlotLayout::data_at, size);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    return __atomic_load_n(word, __ATOMIC_RELAXED) == sequence;
}

bool PublishedSlotReader::invariants_hold() const
{
    AV_SCOPE("PublishedSlotReader::invariants_hold");
    return (slots_ == nullptr) == (count_ == 0u);
}

}
