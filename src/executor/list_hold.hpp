#ifndef AV_LIST_HOLD_HPP
#define AV_LIST_HOLD_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

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

    explicit ListHold(BlockSource &blocks) : held_(blocks, Pool::paged), mark_(0u)
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
        const Status appended = held_.append(held);
        fresh = appended == Status::ok;
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
        if (held_.size() != 0u) held_.truncate(held_.size() - 1u);
    }

    void forget_mark()
    {
        AV_SCOPE("ListHold::forget_mark");
        mark_ = 0u;
    }

    bool holds(const Object *object) const
    {
        AV_SCOPE("ListHold::holds");
        for (uint32_t index = held_.size(); index != 0u; index--)
            if (held_.at(index - 1u).object_ == object) return true;
        return false;
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
    GrowableArray<Held> held_;
    uint64_t            mark_;
};

}

#endif
