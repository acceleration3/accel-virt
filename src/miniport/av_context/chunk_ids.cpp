#include "miniport/av_context/chunk_ids.hpp"

#include "core/platform/platform.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/resources/host_blob_id.hpp"

namespace av {

ChunkIds::ChunkIds(Platform &platform, uint32_t most)
    : platform_(platform), words_(platform, Pool::paged), most_(most), taken_(0u), lowest_(0u)
{
    AV_SCOPE("ChunkIds::ChunkIds");
}

void ChunkIds::mark(uint32_t index)
{
    AV_SCOPE("ChunkIds::mark");
    words_.at(index / word_bits) |= uint64_t{1} << (index % word_bits);
    taken_++;
    lowest_ = index / word_bits;
}

Status ChunkIds::take(uint32_t &id)
{
    AV_SCOPE("ChunkIds::take");
    platform_.require_irql_at_most(Irql::passive, "ChunkIds::take");
    uint32_t word = lowest_;
    while (word < words_.size() && words_.at(word) == ~uint64_t{0}) word++;
    if (word == words_.size() && words_.append(0u) != Status::ok) return Status::no_memory;
    const uint32_t index = word * word_bits + static_cast<uint32_t>(__builtin_ctzll(~words_.at(word)));
    if (index >= most_) return Status::no_space;
    mark(index);
    id = HostBlobId::first + index;
    return Status::ok;
}

void ChunkIds::give_back(uint32_t id)
{
    AV_SCOPE("ChunkIds::give_back");
    if (!HostBlobId(id).valid()) return;
    const uint32_t index = id - HostBlobId::first;
    const uint32_t word = index / word_bits;
    const uint64_t bit = uint64_t{1} << (index % word_bits);
    if (word >= words_.size() || (words_.at(word) & bit) == 0u) return;
    words_.at(word) &= ~bit;
    taken_--;
    if (word < lowest_) lowest_ = word;
}

uint32_t ChunkIds::taken() const
{
    AV_SCOPE("ChunkIds::taken");
    return taken_;
}

bool ChunkIds::invariants_hold() const
{
    AV_SCOPE("ChunkIds::invariants_hold");
    uint32_t counted = 0u;
    for (uint32_t word = 0u; word < words_.size(); word++) {
        const uint64_t bits = words_.at(word);
        counted += static_cast<uint32_t>(__builtin_popcountll(bits));
        if (word < lowest_ && bits != ~uint64_t{0}) return false;
        if (bits != 0u && word * word_bits + (63u - static_cast<uint32_t>(__builtin_clzll(bits))) >= most_)
            return false;
    }
    return counted == taken_ && words_.invariants_hold();
}

}
