#ifndef AV_OBJECT_HOLDS_HPP
#define AV_OBJECT_HOLDS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/list_hold.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

template <typename Object, typename Pin>
class ObjectHolds {
public:
    using Hold = ListHold<Object, Pin>;

    explicit ObjectHolds(BlockSource &blocks)
        : pool_(blocks), lists_(blocks, Pool::paged), retired_(blocks, Pool::paged), blocks_(blocks)
    {
        AV_SCOPE("ObjectHolds::ObjectHolds");
    }
    ObjectHolds(const ObjectHolds &) = delete;
    ObjectHolds &operator=(const ObjectHolds &) = delete;

    [[nodiscard]] Status hold(uint64_t list, Object *object, Pin *pin)
    {
        AV_SCOPE("ObjectHolds::hold");
        Hold *const kept = list_for(list);
        if (kept == nullptr) {
            unpin(pin, 0u);
            return Status::no_memory;
        }
        return place(*kept, object, pin);
    }

    void submitted(uint64_t list, uint64_t mark)
    {
        AV_SCOPE("ObjectHolds::submitted");
        Hold **const found = lists_.find(list);
        if (found != nullptr) (*found)->submitted(mark);
    }

    [[nodiscard]] Status release_list(uint64_t list, uint64_t reached)
    {
        AV_SCOPE("ObjectHolds::release_list");
        Hold **const found = lists_.find(list);
        if (found == nullptr) return Status::ok;
        Hold *const    kept = *found;
        const uint64_t mark = kept->mark();
        while (kept->size() != 0u) {
            const typename Hold::Held held = kept->at(kept->size() - 1u);
            if (mark > reached) {
                const Status deferred = defer(Retired{held.object_, mark});
                if (deferred != Status::ok) return deferred;
            } else {
                held.object_->Release();
            }
            unpin(held.pin_, mark);
            kept->pop();
        }
        static_cast<void>(lists_.erase(list));
        static_cast<void>(pool_.destroy(kept));
        return Status::ok;
    }

    [[nodiscard]] Status retire(Object *object, uint64_t mark, uint64_t reached)
    {
        AV_SCOPE("ObjectHolds::retire");
        if (object == nullptr) return Status::invalid_parameter;
        if (mark <= reached) {
            object->Release();
            return Status::ok;
        }
        return defer(Retired{object, mark});
    }

    [[nodiscard]] Status inherit(const Object *holder, Object *object, Pin *pin)
    {
        AV_SCOPE("ObjectHolds::inherit");
        if (object == nullptr) return Status::invalid_parameter;
        return inherit_from(holder, [&](auto sink) { sink(object, pin); });
    }

    template <typename Producer>
    [[nodiscard]] Status inherit_from(const Object *holder, Producer produce)
    {
        AV_SCOPE("ObjectHolds::inherit_from");
        GrowableArray<Hold *> holding(blocks_, Pool::paged);
        Status                inherited = Status::ok;
        lists_.for_each([&](uint64_t, Hold *&kept) {
            if (inherited == Status::ok && kept->holds(holder)) inherited = holding.append(kept);
        });
        if (inherited != Status::ok || holding.size() == 0u) return inherited;
        produce([&](Object *object, Pin *pin) {
            if (object == nullptr) return;
            for (uint32_t at = 0u; at < holding.size(); at++) {
                if (inherited != Status::ok) return;
                inherited = place(*holding.at(at), object, repinned(pin));
            }
        });
        return inherited;
    }

    void let_go_pin(Pin *pin, uint64_t mark)
    {
        AV_SCOPE("ObjectHolds::let_go_pin");
        unpin(pin, mark);
    }

    void reap(uint64_t reached)
    {
        AV_SCOPE("ObjectHolds::reap");
        while (retired_.size() != 0u && retired_.at(0u).mark_ <= reached) {
            Object *const due = retired_.at(0u).object_;
            pop_due();
            due->Release();
        }
    }

    void release_all()
    {
        AV_SCOPE("ObjectHolds::release_all");
        uint32_t from = 0u;
        for (;;) {
            if (retired_.size() != 0u) {
                Object *const due = retired_.at(retired_.size() - 1u).object_;
                retired_.truncate(retired_.size() - 1u);
                due->Release();
                continue;
            }
            const uint32_t slot = lists_.next_live(from);
            if (slot == KeyedSlots<uint64_t, Hold *>::no_slot) {
                if (from == 0u) break;
                from = 0u;
                continue;
            }
            Hold *const kept = *lists_.at(slot);
            if (kept->size() == 0u) {
                static_cast<void>(lists_.erase(lists_.key_at(slot)));
                static_cast<void>(pool_.destroy(kept));
                from = slot + 1u;
                continue;
            }
            from = slot;
            const typename Hold::Held held = kept->at(kept->size() - 1u);
            kept->pop();
            unpin(held.pin_, 0u);
            held.object_->Release();
        }
        retired_.clear();
        lists_.clear();
        pool_.destroy_all();
    }

    void forget_all()
    {
        AV_SCOPE("ObjectHolds::forget_all");
        retired_.clear();
        lists_.clear();
        pool_.destroy_all();
    }

    template <typename Visitor>
    void for_each_list(Visitor visit) const
    {
        AV_SCOPE("ObjectHolds::for_each_list");
        lists_.for_each([&visit](uint64_t list, Hold *const &) { visit(list); });
    }

    bool holding(uint64_t list, const Object *object) const
    {
        AV_SCOPE("ObjectHolds::holding");
        Hold *const *const found = lists_.find(list);
        return found != nullptr && (*found)->holds(object);
    }

    uint64_t mark_of(uint64_t list) const
    {
        AV_SCOPE("ObjectHolds::mark_of");
        Hold *const *const found = lists_.find(list);
        return found != nullptr ? (*found)->mark() : 0u;
    }

    bool known(uint64_t list) const
    {
        AV_SCOPE("ObjectHolds::known");
        return lists_.find(list) != nullptr;
    }

    template <typename Visitor>
    void for_each_held(uint64_t list, Visitor visit) const
    {
        AV_SCOPE("ObjectHolds::for_each_held");
        Hold *const *const found = lists_.find(list);
        if (found == nullptr) return;
        for (uint32_t index = 0u; index < (*found)->size(); index++) visit((*found)->at(index).object_);
    }

    template <typename Visitor>
    void for_each_pinned(uint64_t list, Visitor visit) const
    {
        AV_SCOPE("ObjectHolds::for_each_pinned");
        Hold *const *const found = lists_.find(list);
        if (found == nullptr) return;
        for (uint32_t index = 0u; index < (*found)->size(); index++)
            visit((*found)->at(index).object_, (*found)->at(index).pin_);
    }

    void let_go_completed(Object *object, uint64_t reached)
    {
        AV_SCOPE("ObjectHolds::let_go_completed");
        lists_.for_each([&](uint64_t, Hold *&kept) {
            if (kept->mark() == 0u || kept->mark() > reached) return;
            typename Hold::Held held = {};
            if (!kept->remove(object, held)) return;
            unpin(held.pin_, kept->mark());
            held.object_->Release();
        });
    }

    [[nodiscard]] Pin *repinned(Pin *pin)
    {
        AV_SCOPE("ObjectHolds::repinned");
        if constexpr (!__is_same(Pin, void)) {
            if (pin != nullptr) pin->took_resource();
        }
        return pin;
    }

    uint32_t lists() const
    {
        AV_SCOPE("ObjectHolds::lists");
        return lists_.count();
    }

    uint32_t retired() const
    {
        AV_SCOPE("ObjectHolds::retired");
        return retired_.size();
    }

    bool invariants_hold() const
    {
        AV_SCOPE("ObjectHolds::invariants_hold");
        if (!pool_.invariants_hold() || !lists_.invariants_hold() || !retired_.invariants_hold()) return false;
        if (pool_.live() != lists_.count()) return false;
        bool sound = true;
        lists_.for_each([&sound](uint64_t, Hold *const &kept) {
            sound = sound && kept != nullptr && kept->invariants_hold();
        });
        for (uint32_t index = 0u; index < retired_.size(); index++) {
            if (retired_.at(index).object_ == nullptr) return false;
            if (index > 0u && retired_.at((index - 1u) / 2u).mark_ > retired_.at(index).mark_) return false;
        }
        return sound;
    }

private:
    struct Retired {
        Object  *object_;
        uint64_t mark_;
    };

    Status defer(const Retired &entry)
    {
        AV_SCOPE("ObjectHolds::defer");
        const Status appended = retired_.append(entry);
        if (appended != Status::ok) return appended;
        uint32_t at = retired_.size() - 1u;
        while (at != 0u && retired_.at((at - 1u) / 2u).mark_ > entry.mark_) {
            retired_.at(at) = retired_.at((at - 1u) / 2u);
            at = (at - 1u) / 2u;
        }
        retired_.at(at) = entry;
        return Status::ok;
    }

    void pop_due()
    {
        AV_SCOPE("ObjectHolds::pop_due");
        const Retired last = retired_.at(retired_.size() - 1u);
        retired_.truncate(retired_.size() - 1u);
        const uint32_t size = retired_.size();
        if (size == 0u) return;
        uint32_t at = 0u;
        for (;;) {
            uint32_t child = at * 2u + 1u;
            if (child >= size) break;
            if (child + 1u < size && retired_.at(child + 1u).mark_ < retired_.at(child).mark_) child++;
            if (retired_.at(child).mark_ >= last.mark_) break;
            retired_.at(at) = retired_.at(child);
            at = child;
        }
        retired_.at(at) = last;
    }

    Status place(Hold &kept, Object *object, Pin *pin)
    {
        AV_SCOPE("ObjectHolds::place");
        bool         fresh = false;
        const Status added = kept.add(typename Hold::Held{object, pin}, fresh);
        if (fresh) {
            object->AddRef();
            return Status::ok;
        }
        if (added == Status::ok && kept.adopt_pin(object, pin)) return Status::ok;
        unpin(pin, 0u);
        return added;
    }

    Hold *list_for(uint64_t list)
    {
        AV_SCOPE("ObjectHolds::list_for");
        Hold **const found = lists_.find(list);
        if (found != nullptr) return *found;
        Hold *made = nullptr;
        if (pool_.create(made, blocks_) != Status::ok) return nullptr;
        uint32_t slot = 0u;
        if (lists_.insert(list, made, slot) == Status::ok) return made;
        static_cast<void>(pool_.destroy(made));
        return nullptr;
    }

    void unpin(Pin *pin, uint64_t mark)
    {
        AV_SCOPE("ObjectHolds::unpin");
        if constexpr (__is_same(Pin, void)) {
            static_cast<void>(pin);
            static_cast<void>(mark);
        } else if (pin != nullptr) {
            pin->let_go_resource(mark);
        }
    }

    ObjectPool<Hold>          pool_;
    KeyedSlots<uint64_t, Hold *> lists_;
    GrowableArray<Retired>    retired_;
    BlockSource              &blocks_;
};

}

#endif
