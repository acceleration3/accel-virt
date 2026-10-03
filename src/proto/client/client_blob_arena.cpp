#include "proto/client/client_blob_arena.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClientBlobLease::ClientBlobLease() : data_(nullptr), offset_(0u), bytes_(0u)
{
    AV_SCOPE("ClientBlobLease::ClientBlobLease");
}

ClientBlobLease::ClientBlobLease(void *data, uint64_t offset, size_t bytes)
    : data_(data), offset_(offset), bytes_(bytes)
    {
        AV_SCOPE("ClientBlobLease::ClientBlobLease");
    }

bool     ClientBlobLease::taken() const
{
    AV_SCOPE("ClientBlobLease::taken");
    return data_ != nullptr;
}
void    *ClientBlobLease::data() const
{
    AV_SCOPE("ClientBlobLease::data");
    return data_;
}
uint64_t ClientBlobLease::offset() const
{
    AV_SCOPE("ClientBlobLease::offset");
    return offset_;
}
size_t   ClientBlobLease::bytes() const
{
    AV_SCOPE("ClientBlobLease::bytes");
    return bytes_;
}

ClientBlobArena::ClientBlobArena() : block_(nullptr), lent_(0u), capacity_(0u), used_(0u)
{
    AV_SCOPE("ClientBlobArena::ClientBlobArena");
}

ClientBlobArena::ClientBlobArena(uint8_t *block, size_t capacity)
    : block_(block), lent_(block == nullptr ? 0u : capacity), capacity_(lent_), used_(0u)
    {
        AV_SCOPE("ClientBlobArena::ClientBlobArena");
    }

void ClientBlobArena::lend(uint8_t *block, size_t capacity)
{
    AV_SCOPE("ClientBlobArena::lend");
    block_ = block;
    lent_ = block == nullptr ? 0u : capacity;
    capacity_ = lent_;
    used_ = 0u;
}

bool ClientBlobArena::ready() const
{
    AV_SCOPE("ClientBlobArena::ready");
    return block_ != nullptr;
}

ClientBlobLease ClientBlobArena::take(size_t count)
{
    AV_SCOPE("ClientBlobArena::take");
    if (count == 0u || count > capacity_ - used_) return ClientBlobLease{};
    const size_t at = used_;
    used_ += count;
    return ClientBlobLease(block_ + at, at, count);
}

void ClientBlobArena::give_back(const ClientBlobLease &lease)
{
    AV_SCOPE("ClientBlobArena::give_back");
    /* Only the top: a bump arena cannot free a lease with another taken after
     * it, and the only lease not yet sent is the last one taken. */
    if (lease.taken() && lease.offset() + lease.bytes() == used_) used_ = static_cast<size_t>(lease.offset());
}

void ClientBlobArena::retire_all()
{
    AV_SCOPE("ClientBlobArena::retire_all");
    used_ = 0u;
}

ClientBlobLease ClientBlobArena::set_aside(size_t count)
{
    AV_SCOPE("ClientBlobArena::set_aside");
    if (count == 0u || count > capacity_ - used_) return ClientBlobLease{};
    capacity_ -= count;
    return ClientBlobLease(block_ + capacity_, capacity_, count);
}

void ClientBlobArena::restore(const ClientBlobLease &aside)
{
    AV_SCOPE("ClientBlobArena::restore");
    if (aside.taken() && aside.offset() == capacity_) capacity_ += aside.bytes();
}

bool ClientBlobArena::holds(const void *bytes, size_t count, uint64_t &offset) const
{
    AV_SCOPE("ClientBlobArena::holds");
    if (block_ == nullptr || count == 0u) return false;
    /* As integers: comparing pointers into different objects is undefined,
     * and `bytes` is any application pointer at all. One below the block
     * wraps to an offset past it. */
    const uintptr_t into = reinterpret_cast<uintptr_t>(bytes) - reinterpret_cast<uintptr_t>(block_);
    if (into > lent_ || count > lent_ - into) return false;
    offset = static_cast<uint64_t>(into);
    return true;
}

size_t ClientBlobArena::used() const
{
    AV_SCOPE("ClientBlobArena::used");
    return used_;
}

size_t ClientBlobArena::room() const
{
    AV_SCOPE("ClientBlobArena::room");
    return capacity_ - used_;
}

bool ClientBlobArena::invariants_hold() const
{
    AV_SCOPE("ClientBlobArena::invariants_hold");
    return used_ <= capacity_ && capacity_ <= lent_ && (block_ != nullptr || lent_ == 0u);
}

}  // namespace av
