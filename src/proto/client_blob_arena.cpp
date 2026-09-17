#include "proto/client_blob_arena.hpp"

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

ClientBlobArena::ClientBlobArena() : block_(nullptr), capacity_(0u), used_(0u)
{
    AV_SCOPE("ClientBlobArena::ClientBlobArena");
}

ClientBlobArena::ClientBlobArena(uint8_t *block, size_t capacity)
    : block_(block), capacity_(block == nullptr ? 0u : capacity), used_(0u)
    {
        AV_SCOPE("ClientBlobArena::ClientBlobArena");
    }

void ClientBlobArena::lend(uint8_t *block, size_t capacity)
{
    AV_SCOPE("ClientBlobArena::lend");
    block_ = block;
    capacity_ = block == nullptr ? 0u : capacity;
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
    if (lease.taken() && lease.offset() + lease.bytes() == used_) used_ = lease.offset();
}

void ClientBlobArena::retire_all()
{
    AV_SCOPE("ClientBlobArena::retire_all");
    used_ = 0u;
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
    return used_ <= capacity_ && (block_ != nullptr || capacity_ == 0u);
}

}  // namespace av
