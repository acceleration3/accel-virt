#include "proto/wire/guest_bytes.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GuestBytes::GuestBytes() : slots_{}, segments_(0u)
{
    AV_SCOPE("GuestBytes::GuestBytes");
}

uint32_t GuestBytes::slots_for(uint64_t length) const
{
    AV_SCOPE("GuestBytes::slots_for");
    return static_cast<uint32_t>((length - 1u) >> slot_shift) + 1u;
}

Status GuestBytes::bind(uint64_t base, uint8_t *memory, uint64_t length)
{
    AV_SCOPE("GuestBytes::bind");
    if (memory == nullptr || length == 0u || base % slot_bytes != 0u) return Status::invalid_parameter;
    const uint64_t first = base >> slot_shift;
    if (first >= slot_count || length > (uint64_t{slot_count} - first) << slot_shift) return Status::invalid_parameter;
    if (base_of(memory) != no_base) return Status::conflict;
    const uint32_t count = slots_for(length);
    for (uint32_t at = 0u; at < count; at++)
        if (slots_[first + at].memory_ != nullptr) return Status::conflict;
    for (uint32_t at = 0u; at < count; at++)
        slots_[first + at] = Slot{memory, length, static_cast<uint32_t>(first)};
    segments_++;
    return Status::ok;
}

Status GuestBytes::unbind(const uint8_t *memory)
{
    AV_SCOPE("GuestBytes::unbind");
    const uint64_t base = base_of(memory);
    if (base == no_base) return Status::not_found;
    const uint32_t first = static_cast<uint32_t>(base >> slot_shift);
    const uint32_t count = slots_for(slots_[first].length_);
    for (uint32_t at = 0u; at < count; at++) slots_[first + at] = Slot{nullptr, 0u, 0u};
    segments_--;
    return Status::ok;
}

Status GuestBytes::resolve(uint64_t offset, uint64_t bytes, uint8_t *&out) const
{
    AV_SCOPE("GuestBytes::resolve");
    out = nullptr;
    const uint64_t slot = offset >> slot_shift;
    if (slot >= slot_count) return Status::invalid_parameter;
    const Slot &held = slots_[slot];
    if (held.memory_ == nullptr) return Status::not_found;
    const uint64_t inside = offset - (uint64_t{held.first_} << slot_shift);
    if (inside > held.length_ || bytes > held.length_ - inside) return Status::invalid_parameter;
    out = held.memory_ + static_cast<uintptr_t>(inside);
    return Status::ok;
}

uint8_t *GuestBytes::at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("GuestBytes::at");
    uint8_t *found = nullptr;
    return resolve(offset, bytes, found) == Status::ok ? found : nullptr;
}

bool GuestBytes::holds(const void *bytes) const
{
    AV_SCOPE("GuestBytes::holds");
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        const Slot &held = slots_[slot];
        if (held.memory_ != nullptr && held.first_ == slot &&
            reinterpret_cast<uintptr_t>(bytes) - reinterpret_cast<uintptr_t>(held.memory_) < held.length_)
            return true;
    }
    return false;
}

uint64_t GuestBytes::segment_bytes(uint32_t slot) const
{
    AV_SCOPE("GuestBytes::segment_bytes");
    return slot < slot_count ? slots_[slot].length_ : 0u;
}

uint64_t GuestBytes::base_of(const uint8_t *memory) const
{
    AV_SCOPE("GuestBytes::base_of");
    if (memory == nullptr) return no_base;
    for (uint32_t slot = 0u; slot < slot_count; slot++)
        if (slots_[slot].memory_ == memory) return uint64_t{slots_[slot].first_} << slot_shift;
    return no_base;
}

uint64_t GuestBytes::base_at(uint64_t offset) const
{
    AV_SCOPE("GuestBytes::base_at");
    const uint64_t slot = offset >> slot_shift;
    if (slot >= slot_count || slots_[slot].memory_ == nullptr) return no_base;
    return uint64_t{slots_[slot].first_} << slot_shift;
}

uint32_t GuestBytes::segments() const
{
    AV_SCOPE("GuestBytes::segments");
    return segments_;
}

bool GuestBytes::invariants_hold() const
{
    AV_SCOPE("GuestBytes::invariants_hold");
    uint32_t firsts = 0u;
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        const Slot &held = slots_[slot];
        if (held.memory_ == nullptr) {
            if (held.length_ != 0u || held.first_ != 0u) return false;
            continue;
        }
        if (held.length_ == 0u || held.first_ > slot || slot - held.first_ >= slots_for(held.length_)) return false;
        if (slots_[held.first_].memory_ != held.memory_ || slots_[held.first_].first_ != held.first_) return false;
        if (held.first_ == slot) firsts++;
    }
    return firsts == segments_;
}

}
