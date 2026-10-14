#include "miniport/av_context/chunk_ids.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ChunkIds::ChunkIds() : taken_{}
{
    AV_SCOPE("ChunkIds::ChunkIds");
}

Status ChunkIds::take(uint32_t &id)
{
    AV_SCOPE("ChunkIds::take");
    for (uint32_t word = 0u; word < words; word++) {
        if (taken_[word] == ~uint64_t{0}) continue;
        const uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(~taken_[word]));
        taken_[word] |= uint64_t{1} << bit;
        id = HostBlobId::first + word * word_bits + bit;
        return Status::ok;
    }
    return Status::no_space;
}

void ChunkIds::give_back(uint32_t id)
{
    AV_SCOPE("ChunkIds::give_back");
    const HostBlobId named(id);
    if (!named.valid()) return;
    taken_[named.slot() / word_bits] &= ~(uint64_t{1} << (named.slot() % word_bits));
}

uint32_t ChunkIds::taken() const
{
    AV_SCOPE("ChunkIds::taken");
    uint32_t count = 0u;
    for (const uint64_t word : taken_) count += static_cast<uint32_t>(__builtin_popcountll(word));
    return count;
}

}  // namespace av
