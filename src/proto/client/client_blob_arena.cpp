#include "proto/client/client_blob_arena.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClientBlobArena::ClientBlobArena() : segments_{}, regions_{}, count_(0u), peak_ring_(0u), window_peak_(0u)
{
    AV_SCOPE("ClientBlobArena::ClientBlobArena");
}

ClientBlobArena::ClientBlobArena(uint8_t *block, size_t capacity) : ClientBlobArena()
{
    AV_SCOPE("ClientBlobArena::ClientBlobArena");
    lend(block, capacity);
}

void ClientBlobArena::lend(uint8_t *block, size_t capacity)
{
    AV_SCOPE("ClientBlobArena::lend");
    for (Segment &segment : segments_) {
        segment.base_ = 0u;
        segment.ring_.lend(nullptr, 0u);
    }
    for (Region &region : regions_) region = Region{0u, nullptr, 0u};
    count_ = 0u;
    peak_ring_ = 0u;
    window_peak_ = 0u;
    if (block != nullptr && capacity != 0u) static_cast<void>(add_segment(0u, block, capacity));
}

bool ClientBlobArena::add_segment(uint64_t base, uint8_t *block, size_t capacity)
{
    AV_SCOPE("ClientBlobArena::add_segment");
    if (block == nullptr || capacity == 0u || count_ == max_segments || base + capacity < base) return false;
    if (overlaps(base, capacity)) return false;
    Segment *free_segment = segments_;
    while (free_segment->ring_.ready()) free_segment++;
    free_segment->base_ = base;
    free_segment->ring_.lend(block, capacity);
    count_++;
    return true;
}

bool ClientBlobArena::remove_segment(uint64_t base)
{
    AV_SCOPE("ClientBlobArena::remove_segment");
    for (Segment &held : segments_) {
        if (!held.ring_.ready() || held.base_ != base || !held.ring_.idle()) continue;
        held.ring_.lend(nullptr, 0u);
        held.base_ = 0u;
        count_--;
        window_peak_ = 0u;
        return true;
    }
    return false;
}

bool ClientBlobArena::add_region(uint64_t base, uint8_t *block, size_t bytes)
{
    AV_SCOPE("ClientBlobArena::add_region");
    if (block == nullptr || bytes == 0u || base + bytes < base || overlaps(base, bytes)) return false;
    for (Region &held : regions_) {
        if (held.block_ != nullptr) continue;
        held = Region{base, block, bytes};
        return true;
    }
    return false;
}

bool ClientBlobArena::remove_region(uint64_t base)
{
    AV_SCOPE("ClientBlobArena::remove_region");
    for (Region &held : regions_) {
        if (held.block_ == nullptr || held.base_ != base) continue;
        held = Region{0u, nullptr, 0u};
        return true;
    }
    return false;
}

bool ClientBlobArena::overlaps(uint64_t base, size_t bytes) const
{
    AV_SCOPE("ClientBlobArena::overlaps");
    for (const Segment &held : segments_)
        if (held.ring_.ready() && base < held.base_ + held.ring_.lent() && held.base_ < base + bytes) return true;
    for (const Region &held : regions_)
        if (held.block_ != nullptr && base < held.base_ + held.bytes_ && held.base_ < base + bytes) return true;
    return false;
}

bool ClientBlobArena::ready() const
{
    AV_SCOPE("ClientBlobArena::ready");
    return count_ != 0u;
}

ClientBlobArena::Segment *ClientBlobArena::segment_at(uint64_t offset)
{
    AV_SCOPE("ClientBlobArena::segment_at");
    for (Segment &held : segments_)
        if (held.ring_.ready() && offset >= held.base_ && offset - held.base_ < held.ring_.lent()) return &held;
    return nullptr;
}

ClientBlobLease ClientBlobArena::take(size_t count)
{
    AV_SCOPE("ClientBlobArena::take");
    if (count == 0u || count_ == 0u) return ClientBlobLease{};
    for (uint32_t tried = 0u; tried < max_segments; tried++) {
        Segment &held = segments_[tried];
        if (!held.ring_.ready()) continue;
        const ClientBlobLease lease = held.ring_.take(count);
        if (!lease.taken()) continue;
        const size_t in_use = used();
        if (in_use > peak_ring_) peak_ring_ = in_use;
        if (in_use > window_peak_) window_peak_ = in_use;
        return ClientBlobLease(lease.data(), held.base_ + lease.offset(), lease.bytes());
    }
    return ClientBlobLease{};
}

void ClientBlobArena::give_back(const ClientBlobLease &lease)
{
    AV_SCOPE("ClientBlobArena::give_back");
    if (!lease.taken()) return;
    Segment *const held = segment_at(lease.offset());
    if (held == nullptr) return;
    held->ring_.give_back(ClientBlobLease(lease.data(), lease.offset() - held->base_, lease.bytes()));
}

void ClientBlobArena::seal()
{
    AV_SCOPE("ClientBlobArena::seal");
    for (Segment &held : segments_)
        if (held.ring_.ready()) held.ring_.seal();
}

void ClientBlobArena::mark(uint64_t fence)
{
    AV_SCOPE("ClientBlobArena::mark");
    for (Segment &held : segments_)
        if (held.ring_.ready()) held.ring_.mark(fence);
}

void ClientBlobArena::retire_through(uint64_t fence)
{
    AV_SCOPE("ClientBlobArena::retire_through");
    for (Segment &held : segments_)
        if (held.ring_.ready()) held.ring_.retire_through(fence);
}

bool ClientBlobArena::holds(const void *bytes, size_t count, uint64_t &offset) const
{
    AV_SCOPE("ClientBlobArena::holds");
    for (const Segment &held : segments_) {
        uint64_t inside = 0u;
        if (!held.ring_.ready() || !held.ring_.holds(bytes, count, inside)) continue;
        offset = held.base_ + inside;
        return true;
    }
    for (const Region &held : regions_) {
        const uintptr_t into = reinterpret_cast<uintptr_t>(bytes) - reinterpret_cast<uintptr_t>(held.block_);
        if (held.block_ == nullptr || count == 0u || into > held.bytes_ || count > held.bytes_ - into) continue;
        offset = held.base_ + into;
        return true;
    }
    return false;
}

bool ClientBlobArena::fits(const uint64_t *sizes, uint32_t wanted) const
{
    AV_SCOPE("ClientBlobArena::fits");
    BlobRing::Placement plans[max_segments];
    for (uint32_t index = 0u; index < max_segments; index++) plans[index] = segments_[index].ring_.placement();
    return placed(plans, sizes, wanted);
}

bool ClientBlobArena::fits_drained(const uint64_t *sizes, uint32_t wanted) const
{
    AV_SCOPE("ClientBlobArena::fits_drained");
    BlobRing::Placement plans[max_segments];
    for (uint32_t index = 0u; index < max_segments; index++) plans[index] = segments_[index].ring_.drained();
    return placed(plans, sizes, wanted);
}

bool ClientBlobArena::placed(BlobRing::Placement *plans, const uint64_t *sizes, uint32_t wanted) const
{
    AV_SCOPE("ClientBlobArena::placed");
    for (uint32_t blob = 0u; blob < wanted; blob++) {
        if (sizes[blob] == 0u) continue;
        if constexpr (sizeof(size_t) < sizeof(uint64_t))
            if (sizes[blob] > ~size_t{0}) return false;
        bool found = false;
        for (uint32_t tried = 0u; tried < max_segments && !found; tried++) {
            size_t         at = 0u;
            bool           wraps = false;
            if (!plans[tried].place(static_cast<size_t>(sizes[blob]), at, wraps)) continue;
            found = true;
        }
        if (!found) return false;
    }
    return true;
}

uint32_t ClientBlobArena::segments() const
{
    AV_SCOPE("ClientBlobArena::segments");
    return count_;
}

uint64_t ClientBlobArena::segment_base(uint32_t index) const
{
    AV_SCOPE("ClientBlobArena::segment_base");
    return index < max_segments ? segments_[index].base_ : 0u;
}

size_t ClientBlobArena::segment_bytes(uint32_t index) const
{
    AV_SCOPE("ClientBlobArena::segment_bytes");
    return index < max_segments ? segments_[index].ring_.lent() : 0u;
}

bool ClientBlobArena::segment_idle(uint32_t index) const
{
    AV_SCOPE("ClientBlobArena::segment_idle");
    return index < max_segments && segments_[index].ring_.idle();
}

bool ClientBlobArena::idle_at(uint64_t base) const
{
    AV_SCOPE("ClientBlobArena::idle_at");
    for (const Segment &held : segments_)
        if (held.ring_.ready() && held.base_ == base) return held.ring_.idle();
    return false;
}

size_t ClientBlobArena::used() const
{
    AV_SCOPE("ClientBlobArena::used");
    size_t total = 0u;
    for (const Segment &held : segments_) total += held.ring_.used();
    return total;
}

size_t ClientBlobArena::room() const
{
    AV_SCOPE("ClientBlobArena::room");
    size_t most = 0u;
    for (const Segment &held : segments_) {
        const size_t here = held.ring_.room();
        if (here > most) most = here;
    }
    return most;
}

size_t ClientBlobArena::ring_bytes() const
{
    AV_SCOPE("ClientBlobArena::ring_bytes");
    size_t total = 0u;
    for (const Segment &held : segments_) total += held.ring_.lent();
    return total;
}

size_t ClientBlobArena::lent() const
{
    AV_SCOPE("ClientBlobArena::lent");
    return ring_bytes() + set_aside_bytes();
}

size_t ClientBlobArena::peak_ring() const
{
    AV_SCOPE("ClientBlobArena::peak_ring");
    return peak_ring_;
}

size_t ClientBlobArena::window_peak() const
{
    AV_SCOPE("ClientBlobArena::window_peak");
    return window_peak_;
}

void ClientBlobArena::restart_window()
{
    AV_SCOPE("ClientBlobArena::restart_window");
    window_peak_ = 0u;
}

size_t ClientBlobArena::set_aside_bytes() const
{
    AV_SCOPE("ClientBlobArena::set_aside_bytes");
    size_t total = 0u;
    for (const Region &held : regions_) total += held.bytes_;
    return total;
}

bool ClientBlobArena::invariants_hold() const
{
    AV_SCOPE("ClientBlobArena::invariants_hold");
    uint32_t live = 0u;
    for (uint32_t index = 0u; index < max_segments; index++) {
        const Segment &held = segments_[index];
        if (!held.ring_.invariants_hold()) return false;
        if (!held.ring_.ready()) {
            if (held.base_ != 0u) return false;
            continue;
        }
        live++;
        for (uint32_t other = index + 1u; other < max_segments; other++) {
            const Segment &next = segments_[other];
            if (next.ring_.ready() && held.base_ < next.base_ + next.ring_.lent() &&
                next.base_ < held.base_ + held.ring_.lent())
                return false;
        }
    }
    for (uint32_t index = 0u; index < max_regions; index++) {
        const Region &held = regions_[index];
        if ((held.block_ == nullptr) != (held.bytes_ == 0u)) return false;
        if (held.block_ == nullptr) continue;
        for (uint32_t other = index + 1u; other < max_regions; other++)
            if (regions_[other].block_ != nullptr && held.base_ < regions_[other].base_ + regions_[other].bytes_ &&
                regions_[other].base_ < held.base_ + held.bytes_)
                return false;
        for (const Segment &ring : segments_)
            if (ring.ring_.ready() && held.base_ < ring.base_ + ring.ring_.lent() && ring.base_ < held.base_ + held.bytes_)
                return false;
    }
    return live == count_ && window_peak_ <= peak_ring_;
}

}
