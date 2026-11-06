#ifndef AV_ICD_OPEN_MAPPINGS_HPP
#define AV_ICD_OPEN_MAPPINGS_HPP

#include "core/platform/block_source.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/staged_mapping.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::icd {

template <typename Mapping>
class OpenMappings {
    static_assert(__is_base_of(StagedMapping, Mapping), "a map is a StagedMapping");

    struct Head {
        uint32_t slot_;
        uint32_t sharers_;
    };
    using Index = KeyedSlots<uint64_t, Head>;

public:
    explicit OpenMappings(BlockSource &memory);
    ~OpenMappings();
    OpenMappings(const OpenMappings &) = delete;
    OpenMappings &operator=(const OpenMappings &) = delete;

    [[nodiscard]] Status stage(const Mapping &mapping, Mapping *&opened);
    [[nodiscard]] Status lend(const Mapping &mapping, uint8_t *at, Mapping *&opened);
    Mapping             *find(uint64_t object, uint32_t subresource);
    const Mapping       *find(uint64_t object, uint32_t subresource) const;
    Mapping             *answering(uint64_t object, const void *pointer);
    void                 close(Mapping &mapping);
    template <typename Predicate>
    uint32_t             close_if(Predicate matches);
    void                 close_all();
    template <typename Visitor>
    void                 for_each(Visitor visit);
    template <typename Visitor>
    void                 for_each(Visitor visit) const;
    uint32_t             count() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status open(const Mapping &mapping, uint8_t *at, StagedMapping::Memory memory, Mapping *&opened);
    uint64_t             pointer_key(const void *pointer) const;
    uint32_t             slot_of(const Mapping &mapping) const;
    void                 close_slot(uint32_t slot);
    void                 index(Index &index, uint64_t key, uint32_t slot);
    template <typename KeyOf>
    void                 unindex(Index &index, uint64_t key, uint32_t slot, KeyOf key_of);
    template <typename Predicate>
    uint32_t             other_slot(uint32_t except, Predicate matches) const;
    template <typename KeyOf>
    bool                 index_holds(const Index &index, KeyOf key_of) const;

    BlockSource                   &memory_;
    KeyedSlots<uint64_t, Mapping> rows_;
    Index                          objects_;
    Index                          pointers_;
    uint64_t                       opened_;

    friend struct OpenMappingsProbe;
};

template <typename Mapping>
OpenMappings<Mapping>::OpenMappings(BlockSource &memory)
    : memory_(memory), rows_(memory, Pool::paged), objects_(memory, Pool::paged), pointers_(memory, Pool::paged),
      opened_(0u)
{
    AV_SCOPE("OpenMappings::OpenMappings");
}

template <typename Mapping>
OpenMappings<Mapping>::~OpenMappings()
{
    AV_SCOPE("OpenMappings::~OpenMappings");
    close_all();
}

template <typename Mapping>
Status OpenMappings<Mapping>::stage(const Mapping &mapping, Mapping *&opened)
{
    AV_SCOPE("OpenMappings::stage");
    opened = nullptr;
    if (mapping.object() == 0u || mapping.bytes() == 0u) return Status::invalid_parameter;
    if constexpr (sizeof(size_t) < sizeof(uint64_t))
        if (mapping.bytes() > static_cast<uint64_t>(~size_t{0})) return Status::invalid_parameter;
    const size_t   bytes = static_cast<size_t>(mapping.bytes());
    uint8_t *const copy = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
    if (copy == nullptr) return Status::no_memory;
    __builtin_memset(copy, 0, bytes);
    const Status made = open(mapping, copy, StagedMapping::Memory::copy, opened);
    if (made != Status::ok) memory_.release(copy);
    return made;
}

template <typename Mapping>
Status OpenMappings<Mapping>::lend(const Mapping &mapping, uint8_t *at, Mapping *&opened)
{
    AV_SCOPE("OpenMappings::lend");
    opened = nullptr;
    if (mapping.object() == 0u || at == nullptr) return Status::invalid_parameter;
    return open(mapping, at, StagedMapping::Memory::named, opened);
}

template <typename Mapping>
Status OpenMappings<Mapping>::open(const Mapping &mapping, uint8_t *at, StagedMapping::Memory memory,
                                   Mapping *&opened)
{
    AV_SCOPE("OpenMappings::open");
    const uint32_t wanted = rows_.count() + 1u;
    if (rows_.reserve(wanted) != Status::ok || objects_.reserve(wanted) != Status::ok ||
        pointers_.reserve(wanted) != Status::ok)
        return Status::no_memory;
    Mapping handed = mapping;
    handed.hand(at, memory);
    uint32_t slot = 0u;
    static_cast<void>(rows_.insert(opened_, handed, slot));
    opened_++;
    index(objects_, handed.object(), slot);
    index(pointers_, pointer_key(at), slot);
    opened = rows_.at(slot);
    return Status::ok;
}

template <typename Mapping>
Mapping *OpenMappings<Mapping>::find(uint64_t object, uint32_t subresource)
{
    AV_SCOPE("OpenMappings::find");
    const Head *const head = objects_.find(object);
    if (head == nullptr) return nullptr;
    Mapping *const first = rows_.at(head->slot_);
    if (first->subresource() == subresource) return first;
    if (head->sharers_ == 1u) return nullptr;
    const uint32_t slot = other_slot(head->slot_, [object, subresource](const Mapping &mapping) {
        return mapping.object() == object && mapping.subresource() == subresource;
    });
    return slot != KeyedSlots<uint64_t, Mapping>::no_slot ? rows_.at(slot) : nullptr;
}

template <typename Mapping>
const Mapping *OpenMappings<Mapping>::find(uint64_t object, uint32_t subresource) const
{
    AV_SCOPE("OpenMappings::find");
    return const_cast<OpenMappings *>(this)->find(object, subresource);
}

template <typename Mapping>
Mapping *OpenMappings<Mapping>::answering(uint64_t object, const void *pointer)
{
    AV_SCOPE("OpenMappings::answering");
    const Head *const head = pointer != nullptr ? pointers_.find(pointer_key(pointer)) : nullptr;
    if (head == nullptr) return nullptr;
    Mapping *const first = rows_.at(head->slot_);
    if (first->object() == object) return first;
    if (head->sharers_ == 1u) return nullptr;
    const uint32_t slot = other_slot(head->slot_, [object](const Mapping &mapping) {
        return mapping.object() == object;
    });
    return slot != KeyedSlots<uint64_t, Mapping>::no_slot ? rows_.at(slot) : nullptr;
}

template <typename Mapping>
void OpenMappings<Mapping>::close(Mapping &mapping)
{
    AV_SCOPE("OpenMappings::close");
    const uint32_t slot = slot_of(mapping);
    if (slot != KeyedSlots<uint64_t, Mapping>::no_slot) close_slot(slot);
}

template <typename Mapping>
template <typename Predicate>
uint32_t OpenMappings<Mapping>::close_if(Predicate matches)
{
    AV_SCOPE("OpenMappings::close_if");
    uint32_t closed = 0u;
    for (uint32_t slot = rows_.next_live(0u); slot != rows_.no_slot; slot = rows_.next_live(slot + 1u)) {
        if (!matches(*rows_.at(slot))) continue;
        close_slot(slot);
        closed++;
    }
    return closed;
}

template <typename Mapping>
void OpenMappings<Mapping>::close_all()
{
    AV_SCOPE("OpenMappings::close_all");
    static_cast<void>(close_if([](const Mapping &) { return true; }));
}

template <typename Mapping>
template <typename Visitor>
void OpenMappings<Mapping>::for_each(Visitor visit)
{
    AV_SCOPE("OpenMappings::for_each");
    rows_.for_each([&visit](uint64_t, Mapping &mapping) { visit(mapping); });
}

template <typename Mapping>
template <typename Visitor>
void OpenMappings<Mapping>::for_each(Visitor visit) const
{
    AV_SCOPE("OpenMappings::for_each");
    rows_.for_each([&visit](uint64_t, const Mapping &mapping) { visit(mapping); });
}

template <typename Mapping>
uint32_t OpenMappings<Mapping>::count() const
{
    AV_SCOPE("OpenMappings::count");
    return rows_.count();
}

template <typename Mapping>
bool OpenMappings<Mapping>::invariants_hold() const
{
    AV_SCOPE("OpenMappings::invariants_hold");
    bool whole = true;
    rows_.for_each([&whole](uint64_t, const Mapping &mapping) {
        whole = whole && mapping.invariants_hold() && mapping.at() != nullptr;
    });
    return whole && rows_.invariants_hold() && objects_.invariants_hold() && pointers_.invariants_hold() &&
           index_holds(objects_, [](const Mapping &mapping) { return mapping.object(); }) &&
           index_holds(pointers_, [this](const Mapping &mapping) { return pointer_key(mapping.at()); });
}

template <typename Mapping>
uint64_t OpenMappings<Mapping>::pointer_key(const void *pointer) const
{
    AV_SCOPE("OpenMappings::pointer_key");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer));
}

template <typename Mapping>
uint32_t OpenMappings<Mapping>::slot_of(const Mapping &mapping) const
{
    AV_SCOPE("OpenMappings::slot_of");
    const Head *const head = objects_.find(mapping.object());
    if (head == nullptr) return rows_.no_slot;
    if (rows_.at(head->slot_) == &mapping) return head->slot_;
    return other_slot(head->slot_, [&mapping](const Mapping &live) { return &live == &mapping; });
}

template <typename Mapping>
void OpenMappings<Mapping>::close_slot(uint32_t slot)
{
    AV_SCOPE("OpenMappings::close_slot");
    Mapping &mapping = *rows_.at(slot);
    unindex(objects_, mapping.object(), slot, [](const Mapping &other) { return other.object(); });
    unindex(pointers_, pointer_key(mapping.at()), slot,
            [this](const Mapping &other) { return pointer_key(other.at()); });
    if (mapping.copied()) memory_.release(mapping.at());
    rows_.erase_slot(slot);
}

template <typename Mapping>
void OpenMappings<Mapping>::index(Index &index, uint64_t key, uint32_t slot)
{
    AV_SCOPE("OpenMappings::index");
    Head *const head = index.find(key);
    if (head != nullptr) {
        head->sharers_++;
        return;
    }
    uint32_t placed = 0u;
    static_cast<void>(index.insert(key, Head{slot, 1u}, placed));
}

template <typename Mapping>
template <typename KeyOf>
void OpenMappings<Mapping>::unindex(Index &index, uint64_t key, uint32_t slot, KeyOf key_of)
{
    AV_SCOPE("OpenMappings::unindex");
    Head *const head = index.find(key);
    if (head->sharers_ == 1u) {
        static_cast<void>(index.erase(key));
        return;
    }
    head->sharers_--;
    if (head->slot_ == slot)
        head->slot_ = other_slot(slot, [&key_of, key](const Mapping &other) { return key_of(other) == key; });
}

template <typename Mapping>
template <typename Predicate>
uint32_t OpenMappings<Mapping>::other_slot(uint32_t except, Predicate matches) const
{
    AV_SCOPE("OpenMappings::other_slot");
    for (uint32_t slot = rows_.next_live(0u); slot != rows_.no_slot; slot = rows_.next_live(slot + 1u))
        if (slot != except && matches(*rows_.at(slot))) return slot;
    return rows_.no_slot;
}

template <typename Mapping>
template <typename KeyOf>
bool OpenMappings<Mapping>::index_holds(const Index &index, KeyOf key_of) const
{
    AV_SCOPE("OpenMappings::index_holds");
    uint32_t counted = 0u;
    bool     first_under_key = true;
    index.for_each([this, &counted, &first_under_key, &key_of](uint64_t key, const Head &head) {
        const Mapping *const first = rows_.at(head.slot_);
        first_under_key = first_under_key && head.sharers_ != 0u && first != nullptr && key_of(*first) == key;
        counted += head.sharers_;
    });
    return first_under_key && counted == rows_.count();
}

}

#endif
