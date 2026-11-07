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
        bool         fresh = false;
        const Status added = kept->add(typename Hold::Held{object, pin}, fresh);
        if (fresh) {
            object->AddRef();
            return Status::ok;
        }
        unpin(pin, 0u);
        return added;
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
                const Status deferred = retired_.append(Retired{held.object_, mark});
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
        return retired_.append(Retired{object, mark});
    }

    [[nodiscard]] Status inherit(const Object *holder, Object *object)
    {
        AV_SCOPE("ObjectHolds::inherit");
        if (object == nullptr) return Status::invalid_parameter;
        Status inherited = Status::ok;
        lists_.for_each([&](uint64_t, Hold *&kept) {
            if (inherited != Status::ok || !kept->holds(holder)) return;
            bool         fresh = false;
            const Status added = kept->add(typename Hold::Held{object, nullptr}, fresh);
            if (fresh) object->AddRef();
            inherited = added;
        });
        return inherited;
    }

    void reap(uint64_t reached)
    {
        AV_SCOPE("ObjectHolds::reap");
        uint32_t index = 0u;
        while (index < retired_.size()) {
            const Retired due = retired_.at(index);
            if (due.mark_ > reached) {
                index++;
                continue;
            }
            due.object_->Release();
            retired_.remove_swap(index);
        }
    }

    void release_all()
    {
        AV_SCOPE("ObjectHolds::release_all");
        while (retired_.size() != 0u) {
            const Retired due = retired_.at(retired_.size() - 1u);
            retired_.truncate(retired_.size() - 1u);
            due.object_->Release();
        }
        for (uint32_t slot = lists_.next_live(0u); slot != KeyedSlots<uint64_t, Hold *>::no_slot;
             slot = lists_.next_live(0u))
            static_cast<void>(release_list(lists_.key_at(slot), ~uint64_t{0}));
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
        for (const Retired &due : retired_)
            if (due.object_ == nullptr) return false;
        return sound;
    }

private:
    struct Retired {
        Object  *object_;
        uint64_t mark_;
    };

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
