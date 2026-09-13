#include "proto/client_blob_arena.hpp"

namespace av {

ClientBlobLease::ClientBlobLease() : data_(nullptr), offset_(0u), bytes_(0u) {}

ClientBlobLease::ClientBlobLease(void *data, uint64_t offset, size_t bytes)
    : data_(data), offset_(offset), bytes_(bytes) {}

bool     ClientBlobLease::taken() const { return data_ != nullptr; }
void    *ClientBlobLease::data() const { return data_; }
uint64_t ClientBlobLease::offset() const { return offset_; }
size_t   ClientBlobLease::bytes() const { return bytes_; }

ClientBlobArena::ClientBlobArena() : block_(nullptr), capacity_(0u), used_(0u) {}

ClientBlobArena::ClientBlobArena(uint8_t *block, size_t capacity)
    : block_(block), capacity_(block == nullptr ? 0u : capacity), used_(0u) {}

bool ClientBlobArena::ready() const { return block_ != nullptr; }

ClientBlobLease ClientBlobArena::take(size_t count)
{
    if (count == 0u || count > capacity_ - used_) return ClientBlobLease{};
    const size_t at = used_;
    used_ += count;
    return ClientBlobLease(block_ + at, at, count);
}

void ClientBlobArena::give_back(const ClientBlobLease &lease)
{
    /* Only the top: a bump arena cannot free a lease with another taken after
     * it, and the only lease not yet sent is the last one taken. */
    if (lease.taken() && lease.offset() + lease.bytes() == used_) used_ = lease.offset();
}

size_t ClientBlobArena::used() const { return used_; }

bool ClientBlobArena::invariants_hold() const
{
    return used_ <= capacity_ && (block_ != nullptr || capacity_ == 0u);
}

}  // namespace av
