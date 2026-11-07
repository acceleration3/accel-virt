#ifndef AV_LIST_HOLD_HPP
#define AV_LIST_HOLD_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;

template <typename Object, typename Pin>
class ListHold {
public:
    struct Held {
        Object *object_;
        Pin    *pin_;
    };

    explicit ListHold(BlockSource &blocks) : held_(blocks, Pool::paged), index_(blocks, Pool::paged), mark_(0u), stale_(false)
    {
        AV_SCOPE("ListHold::ListHold");
    }
    ListHold(const ListHold &) = delete;
    ListHold &operator=(const ListHold &) = delete;

    [[nodiscard]] Status add(const Held &held, bool &fresh)
    {
        AV_SCOPE("ListHold::add");
        fresh = false;
        if (held.object_ == nullptr) return Status::invalid_parameter;
        if (holds(held.object_)) return Status::ok;
        const Status room = make_room();
        if (room != Status::ok) return room;
        const Status appended = held_.append(held);
        fresh = appended == Status::ok;
        if (fresh) place(held_.size() - 1u);
        return appended;
    }

    void submitted(uint64_t mark)
    {
        AV_SCOPE("ListHold::submitted");
        if (mark > mark_) mark_ = mark;
    }

    void pop()
    {
        AV_SCOPE("ListHold::pop");
        if (held_.size() == 0u) return;
        held_.truncate(held_.size() - 1u);
        stale_ = true;
    }

    void forget_mark()
    {
        AV_SCOPE("ListHold::forget_mark");
        mark_ = 0u;
    }

    bool holds(const Object *object) const
    {
        AV_SCOPE("ListHold::holds");
        if (held_.size() == 0u) return false;
        if (stale_ || index_.size() == 0u) {
            for (uint32_t index = 0u; index < held_.size(); index++)
                if (held_.at(index).object_ == object) return true;
            return false;
        }
        const uint32_t mask = index_.size() - 1u;
        for (uint32_t probe = hash_of(object) & mask;; probe = (probe + 1u) & mask) {
            const uint32_t entry = index_.at(probe);
            if (entry == 0u) return false;
            if (held_.at(entry - 1u).object_ == object) return true;
        }
    }

    uint64_t mark() const
    {
        AV_SCOPE("ListHold::mark");
        return mark_;
    }

    uint32_t size() const
    {
        AV_SCOPE("ListHold::size");
        return held_.size();
    }

    const Held &at(uint32_t index) const
    {
        AV_SCOPE("ListHold::at");
        return held_.at(index);
    }

    bool invariants_hold() const
    {
        AV_SCOPE("ListHold::invariants_hold");
        if (!held_.invariants_hold()) return false;
        for (uint32_t index = 0u; index < held_.size(); index++) {
            if (held_.at(index).object_ == nullptr) return false;
            for (uint32_t other = index + 1u; other < held_.size(); other++)
                if (held_.at(other).object_ == held_.at(index).object_) return false;
        }
        return true;
    }

private:
    static constexpr uint32_t first_index = 16u;

    uint32_t hash_of(const Object *object) const
    {
        AV_SCOPE("ListHold::hash_of");
        return static_cast<uint32_t>((reinterpret_cast<uintptr_t>(object) >> 4u) * 2654435761u);
    }

    void place(uint32_t at)
    {
        AV_SCOPE("ListHold::place");
        const uint32_t mask = index_.size() - 1u;
        uint32_t       probe = hash_of(held_.at(at).object_) & mask;
        while (index_.at(probe) != 0u) probe = (probe + 1u) & mask;
        index_.at(probe) = at + 1u;
    }

    Status rebuild(uint32_t slots)
    {
        AV_SCOPE("ListHold::rebuild");
        index_.clear();
        stale_ = true;
        const Status sized = index_.resize(slots, 0u);
        if (sized != Status::ok) return sized;
        for (uint32_t at = 0u; at < held_.size(); at++) place(at);
        stale_ = false;
        return Status::ok;
    }

    Status make_room()
    {
        AV_SCOPE("ListHold::make_room");
        if (stale_ || index_.size() == 0u) {
            uint32_t slots = first_index;
            while (slots < (held_.size() + 1u) * 2u) slots *= 2u;
            return rebuild(slots);
        }
        if ((held_.size() + 1u) * 2u <= index_.size()) return Status::ok;
        return rebuild(index_.size() * 2u);
    }

    GrowableArray<Held>     held_;
    GrowableArray<uint32_t> index_;
    uint64_t                mark_;
    bool                    stale_;
};

}

#endif
