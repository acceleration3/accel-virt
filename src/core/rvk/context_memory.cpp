#include "core/rvk/context_memory.hpp"

#include "core/support/scope_exit.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

ContextMemory::ContextMemory(Platform &platform)
    : platform_(platform), ring_(nullptr), fence_(nullptr), reply_(nullptr), ring_bytes_(0u), order_(0u) {}

ContextMemory::~ContextMemory() { release(); }

uint8_t *ContextMemory::allocate_block(size_t bytes)
{
    auto *const block = static_cast<uint8_t *>(platform_.allocate_contiguous(bytes));
    if (block != nullptr) platform_.zero(block, bytes);
    return block;
}

Status ContextMemory::allocate(uint8_t order)
{
    platform_.require_irql_at_most(Irql::passive, "ContextMemory::allocate");
    if (ring_ != nullptr) return Status::invalid_state;
    if (order < RingHeader::lowest_order || order > RingHeader::highest_order) return Status::invalid_parameter;
    const size_t ring_bytes = (RingHeader::encoded_size + (size_t{1} << order) + page_bytes - 1u) & ~(page_bytes - 1u);
    ScopeExit    release_all([this] { release(); });
    ring_ = allocate_block(ring_bytes);
    ring_bytes_ = ring_ != nullptr ? ring_bytes : 0u;
    fence_ = ring_ != nullptr ? allocate_block(page_bytes) : nullptr;
    reply_ = fence_ != nullptr ? allocate_block(page_bytes) : nullptr;
    if (reply_ == nullptr) return Status::no_memory;
    release_all.dismiss();
    order_ = order;
    /* Sized for it: a ring holds its header. */
    static_cast<void>(RingHeader(order, 0u, 0u).encode(ring_, RingHeader::encoded_size));
    return Status::ok;
}

void ContextMemory::release()
{
    platform_.require_irql_at_most(Irql::passive, "ContextMemory::release");
    if (reply_ != nullptr) platform_.release_contiguous(reply_, page_bytes);
    if (fence_ != nullptr) platform_.release_contiguous(fence_, page_bytes);
    if (ring_ != nullptr) platform_.release_contiguous(ring_, ring_bytes_);
    ring_ = nullptr;
    fence_ = nullptr;
    reply_ = nullptr;
    ring_bytes_ = 0u;
    order_ = 0u;
}

bool     ContextMemory::allocated() const    { return ring_ != nullptr; }
uint8_t  ContextMemory::order() const        { return order_; }
uint8_t *ContextMemory::ring() const         { return ring_; }
size_t   ContextMemory::ring_bytes() const   { return ring_bytes_; }
uint8_t *ContextMemory::fence_page() const   { return fence_; }
uint8_t *ContextMemory::reply() const        { return reply_; }
uint64_t ContextMemory::ring_address() const { return platform_.physical_address(ring_); }
uint64_t ContextMemory::fence_address() const { return platform_.physical_address(fence_); }
uint64_t ContextMemory::reply_address() const { return platform_.physical_address(reply_); }

/* All three blocks are held, or none is. */
bool ContextMemory::invariants_hold() const
{
    const bool ring = ring_ != nullptr;
    return (fence_ != nullptr) == ring && (reply_ != nullptr) == ring && (ring_bytes_ != 0u) == ring;
}

}  // namespace av
