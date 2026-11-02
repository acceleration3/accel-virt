#include "proto/wire/wire_arena.hpp"

#include "core/platform/block_source.hpp"

namespace av {

WireArena::WireArena(uint8_t *block, size_t capacity)
    : block_(block), capacity_(block == nullptr ? 0u : capacity), room_(block_), room_capacity_(capacity_),
      used_(0u), taken_before_(0u), newest_chunk_(nullptr), chunks_(0u), more_(nullptr), refusals_(0u) {}

WireArena::~WireArena() { give_back_chunks(); }

void *WireArena::claim(size_t count, size_t size, size_t alignment)
{
    if (count == 0u) return refuse();
    if (count > ~size_t{0} / size) return refuse();
    const size_t bytes = count * size;
    const size_t padding = (alignment - (used_ % alignment)) % alignment;
    const bool   fits = padding <= room_capacity_ - used_ && bytes <= room_capacity_ - used_ - padding;
    if (!fits && !take_chunk(bytes, alignment)) return refuse();
    const size_t start = fits ? used_ + padding : 0u;
    used_ = start + bytes;
    __builtin_memset(room_ + start, 0, bytes);
    return room_ + start;
}

bool WireArena::take_chunk(size_t bytes, size_t alignment)
{
    if (more_ == nullptr || alignment > chunk_header || bytes > ~size_t{0} - chunk_header) return false;
    const size_t wanted = bytes > least_chunk ? bytes : least_chunk;
    auto *const  chunk = static_cast<uint8_t *>(more_->allocate(chunk_header + wanted, Pool::paged));
    if (chunk == nullptr) return false;
    __builtin_memcpy(chunk, &newest_chunk_, sizeof newest_chunk_);
    newest_chunk_ = chunk;
    chunks_++;
    taken_before_ += used_;
    room_ = chunk + chunk_header;
    room_capacity_ = wanted;
    used_ = 0u;
    return true;
}

void WireArena::give_back_chunks()
{
    while (newest_chunk_ != nullptr) {
        uint8_t *older = nullptr;
        __builtin_memcpy(&older, newest_chunk_, sizeof older);
        more_->release(newest_chunk_);
        newest_chunk_ = older;
    }
    chunks_ = 0u;
    taken_before_ = 0u;
    room_ = block_;
    room_capacity_ = capacity_;
}

char *WireArena::allocate_chars(size_t count)
{
    return static_cast<char *>(claim(count, sizeof(char), alignof(char)));
}

void WireArena::attach(uint8_t *block, size_t capacity)
{
    give_back_chunks();
    block_ = block;
    capacity_ = block == nullptr ? 0u : capacity;
    room_ = block_;
    room_capacity_ = capacity_;
    used_ = 0u;
}

void WireArena::overflow_into(BlockSource &more)
{
    give_back_chunks();
    used_ = 0u;
    more_ = &more;
}

void *WireArena::refuse()
{
    refusals_++;
    return nullptr;
}

void WireArena::reset()
{
    give_back_chunks();
    used_ = 0u;
}

size_t WireArena::used() const { return taken_before_ + used_; }
size_t WireArena::capacity() const { return capacity_; }
size_t WireArena::chunks() const { return chunks_; }
size_t WireArena::refusals() const { return refusals_; }

bool WireArena::invariants_hold() const
{
    return used_ <= room_capacity_ && (newest_chunk_ == nullptr) == (chunks_ == 0u) &&
           (chunks_ == 0u || more_ != nullptr) && (chunks_ != 0u || room_ == block_);
}

}
