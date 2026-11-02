#ifndef AV_OBJECT_POOL_HPP
#define AV_OBJECT_POOL_HPP

#include "core/platform/block_source.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <new>
#include <stddef.h>
#include <stdint.h>

namespace av {

struct ObjectPoolProbe;

template <typename T>
class ObjectPool {
public:
    static constexpr uint32_t chunk_slots = 64u;

    explicit ObjectPool(BlockSource &platform) : platform_(platform), chunks_(nullptr), live_(0u)
    {
        AV_SCOPE("ObjectPool::ObjectPool");
    }
    ~ObjectPool()
    {
        AV_SCOPE("ObjectPool::~ObjectPool");
        destroy_all();
    }
    ObjectPool(const ObjectPool &) = delete;
    ObjectPool &operator=(const ObjectPool &) = delete;

    template <typename... Arguments>
    [[nodiscard]] Status create(T *&object, Arguments &&...arguments)
    {
        AV_SCOPE("ObjectPool::create");
        platform_.require_irql_at_most(Irql::apc, "ObjectPool::create");
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
        AV_SCOPE("ObjectPool::destroy");
        platform_.require_irql_at_most(Irql::apc, "ObjectPool::destroy");
        Chunk   *previous = nullptr;
        uint32_t slot = 0u;
        Chunk   *chunk = locate(object, previous, slot);
        if (chunk == nullptr) return Status::invalid_parameter;
        release_slot(chunk, previous, slot);
        return Status::ok;
    }

    T *find(void *address) const
    {
        AV_SCOPE("ObjectPool::find");
        platform_.require_irql_at_most(Irql::dispatch, "ObjectPool::find");
        Chunk   *previous = nullptr;
        uint32_t slot = 0u;
        return locate(address, previous, slot) != nullptr ? static_cast<T *>(address) : nullptr;
    }

    template <typename Predicate>
    void destroy_if(Predicate matches)
    {
        AV_SCOPE("ObjectPool::destroy_if");
        platform_.require_irql_at_most(Irql::apc, "ObjectPool::destroy_if");
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

    template <typename Predicate>
    T *find_if(Predicate matches)
    {
        AV_SCOPE("ObjectPool::find_if");
        platform_.require_irql_at_most(Irql::dispatch, "ObjectPool::find_if");
        for (Chunk *chunk = chunks_; chunk != nullptr; chunk = chunk->next_)
            for (uint32_t slot = 0u; slot < chunk_slots; slot++) {
                if ((chunk->used_ & (uint64_t{1} << slot)) == 0u) continue;
                T *const object = object_at(*chunk, slot);
                if (matches(static_cast<const T &>(*object))) return object;
            }
        return nullptr;
    }

    void destroy_all()
    {
        AV_SCOPE("ObjectPool::destroy_all");
        destroy_if([](const T &) { return true; });
    }

    template <typename Predicate>
    bool all_of(Predicate holds) const
    {
        AV_SCOPE("ObjectPool::all_of");
        platform_.require_irql_at_most(Irql::dispatch, "ObjectPool::all_of");
        for (Chunk *chunk = chunks_; chunk != nullptr; chunk = chunk->next_)
            for (uint32_t slot = 0u; slot < chunk_slots; slot++)
                if ((chunk->used_ & (uint64_t{1} << slot)) != 0u &&
                    !holds(static_cast<const T &>(*object_at(*chunk, slot))))
                    return false;
        return true;
    }

    template <typename Visitor>
    void for_each(Visitor visit) const
    {
        AV_SCOPE("ObjectPool::for_each");
        platform_.require_irql_at_most(Irql::dispatch, "ObjectPool::for_each");
        for (Chunk *chunk = chunks_; chunk != nullptr; chunk = chunk->next_)
            for (uint32_t slot = 0u; slot < chunk_slots; slot++)
                if ((chunk->used_ & (uint64_t{1} << slot)) != 0u) visit(static_cast<const T &>(*object_at(*chunk, slot)));
    }

    uint32_t live() const
    {
        AV_SCOPE("ObjectPool::live");
        return live_;
    }

    bool invariants_hold() const
    {
        AV_SCOPE("ObjectPool::invariants_hold");
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

    BlockSource &platform_;
    Chunk    *chunks_;
    uint32_t  live_;

    struct Chunk {
        Chunk   *next_;
        uint64_t used_;
        alignas(T) unsigned char slots_[sizeof(T) * chunk_slots];
    };

    static constexpr uint64_t all_used = ~uint64_t{0};

    void *slot_address(Chunk &chunk, uint32_t slot) const
    {
        AV_SCOPE("ObjectPool::slot_address");
        return chunk.slots_ + sizeof(T) * slot;
    }
    T    *object_at(Chunk &chunk, uint32_t slot) const
    {
        AV_SCOPE("ObjectPool::object_at");
        return static_cast<T *>(slot_address(chunk, slot));
    }

    uint32_t first_free(const Chunk &chunk) const
    {
        AV_SCOPE("ObjectPool::first_free");
        return static_cast<uint32_t>(__builtin_ctzll(~chunk.used_));
    }

    Chunk *chunk_with_room()
    {
        AV_SCOPE("ObjectPool::chunk_with_room");
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
        AV_SCOPE("ObjectPool::locate");
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
        AV_SCOPE("ObjectPool::release_slot");
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

}

#endif
