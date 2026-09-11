/* Objects of one type whose count has no fixed bound - devices, allocations -
 * constructed in place in chunks of non-paged memory, each at an address that
 * stays valid until the object is destroyed, so the address can be the handle
 * dxgkrnl passes back. One of the two places a placement-new expression may
 * appear (constitution III; tools/check_design.py).
 *
 * find accepts any address and answers null unless it is a live object of this
 * pool: a handle from outside is checked before it is trusted. The pool takes
 * no lock; its owner serializes it. */
#ifndef AV_OBJECT_POOL_HPP
#define AV_OBJECT_POOL_HPP

#include "platform.hpp"
#include "status.hpp"

#include <new>
#include <stddef.h>
#include <stdint.h>

namespace av {

struct ObjectPoolProbe;

template <typename T>
class ObjectPool {
public:
    static constexpr uint32_t chunk_slots = 64u;

    explicit ObjectPool(Platform &platform) : platform_(platform), chunks_(nullptr), live_(0u) {}
    ~ObjectPool() { destroy_all(); }
    ObjectPool(const ObjectPool &) = delete;
    ObjectPool &operator=(const ObjectPool &) = delete;

    template <typename... Arguments>
    [[nodiscard]] Status create(T *&object, Arguments &&...arguments)
    {
        platform_.require_irql_at_most(Irql::passive, "ObjectPool::create");
        object = nullptr;
        Chunk *chunk = chunk_with_room();
        if (chunk == nullptr) return Status::no_memory;
        const uint32_t slot = first_free(*chunk);
        object = ::new (slot_address(*chunk, slot)) T(static_cast<Arguments &&>(arguments)...);
        chunk->used_ |= uint64_t{1} << slot;
        live_++;
        return Status::ok;
    }

    [[nodiscard]] Status destroy(T *object)
    {
        platform_.require_irql_at_most(Irql::passive, "ObjectPool::destroy");
        Chunk   *previous = nullptr;
        uint32_t slot = 0u;
        Chunk   *chunk = locate(object, previous, slot);
        if (chunk == nullptr) return Status::invalid_parameter;
        release_slot(chunk, previous, slot);
        return Status::ok;
    }

    /* Legal at DISPATCH: it neither allocates nor releases. */
    T *find(void *address) const
    {
        platform_.require_irql_at_most(Irql::dispatch, "ObjectPool::find");
        Chunk   *previous = nullptr;
        uint32_t slot = 0u;
        return locate(address, previous, slot) != nullptr ? static_cast<T *>(address) : nullptr;
    }

    template <typename Predicate>
    void destroy_if(Predicate matches)
    {
        platform_.require_irql_at_most(Irql::passive, "ObjectPool::destroy_if");
        Chunk *previous = nullptr;
        Chunk *chunk = chunks_;
        while (chunk != nullptr) {
            Chunk *const next = chunk->next_;
            bool released = false;
            for (uint32_t slot = 0u; slot < chunk_slots && !released; slot++) {
                if ((chunk->used_ & (uint64_t{1} << slot)) == 0u) continue;
                if (!matches(static_cast<const T &>(*object_at(*chunk, slot)))) continue;
                released = chunk->used_ == (uint64_t{1} << slot);
                release_slot(chunk, previous, slot);
            }
            if (!released) previous = chunk;
            chunk = next;
        }
    }

    void destroy_all()
    {
        destroy_if([](const T &) { return true; });
    }

    uint32_t live() const { return live_; }

    bool invariants_hold() const
    {
        uint32_t counted = 0u;
        bool     no_empty_chunk = true;
        for (const Chunk *chunk = chunks_; chunk != nullptr; chunk = chunk->next_) {
            counted += static_cast<uint32_t>(__builtin_popcountll(chunk->used_));
            no_empty_chunk = no_empty_chunk && chunk->used_ != 0u;
        }
        return counted == live_ && no_empty_chunk;
    }

private:
    struct Chunk;

    Platform &platform_;
    Chunk    *chunks_;
    uint32_t  live_;

    struct Chunk {
        Chunk   *next_;
        uint64_t used_;
        alignas(T) unsigned char slots_[sizeof(T) * chunk_slots];
    };

    static constexpr uint64_t all_used = ~uint64_t{0};

    void *slot_address(Chunk &chunk, uint32_t slot) const { return chunk.slots_ + sizeof(T) * slot; }
    T    *object_at(Chunk &chunk, uint32_t slot) const { return static_cast<T *>(slot_address(chunk, slot)); }

    uint32_t first_free(const Chunk &chunk) const
    {
        return static_cast<uint32_t>(__builtin_ctzll(~chunk.used_));
    }

    Chunk *chunk_with_room()
    {
        for (Chunk *chunk = chunks_; chunk != nullptr; chunk = chunk->next_)
            if (chunk->used_ != all_used) return chunk;
        void *block = platform_.allocate(sizeof(Chunk), Pool::non_paged);
        if (block == nullptr) return nullptr;
        Chunk *chunk = ::new (block) Chunk{chunks_, 0u, {}};
        chunks_ = chunk;
        return chunk;
    }

    Chunk *locate(void *address, Chunk *&previous, uint32_t &slot) const
    {
        const auto target = reinterpret_cast<uintptr_t>(address);
        previous = nullptr;
        for (Chunk *chunk = chunks_; chunk != nullptr; previous = chunk, chunk = chunk->next_) {
            const auto first = reinterpret_cast<uintptr_t>(chunk->slots_);
            if (target < first || target >= first + sizeof chunk->slots_) continue;
            if ((target - first) % sizeof(T) != 0u) return nullptr;
            slot = static_cast<uint32_t>((target - first) / sizeof(T));
            return (chunk->used_ & (uint64_t{1} << slot)) != 0u ? chunk : nullptr;
        }
        return nullptr;
    }

    void release_slot(Chunk *chunk, Chunk *previous, uint32_t slot)
    {
        object_at(*chunk, slot)->~T();
        chunk->used_ &= ~(uint64_t{1} << slot);
        live_--;
        if (chunk->used_ != 0u) return;
        if (previous == nullptr) chunks_ = chunk->next_;
        else previous->next_ = chunk->next_;
        platform_.release(chunk);
    }

    friend struct ObjectPoolProbe;
};

}  // namespace av

#endif  // AV_OBJECT_POOL_HPP
