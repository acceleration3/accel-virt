#ifndef AV_ICD_LINK_GENERATIONS_HPP
#define AV_ICD_LINK_GENERATIONS_HPP

#include "core/platform/block_source.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av::icd {

template <typename T>
class LinkGenerations {
public:
    explicit LinkGenerations(BlockSource &memory);
    LinkGenerations(const LinkGenerations &) = delete;
    LinkGenerations &operator=(const LinkGenerations &) = delete;

    template <typename... Arguments>
    T   *join(bool &left_lost, Arguments &&...arguments);
    void drop(T &generation);
    uint32_t count() const;
    template <typename Visitor>
    void for_each(Visitor visit) const;

    bool invariants_hold() const;

private:
    ObjectPool<T> generations_;
    T            *current_;
};

template <typename T>
LinkGenerations<T>::LinkGenerations(BlockSource &memory) : generations_(memory), current_(nullptr)
{
    AV_SCOPE("LinkGenerations::LinkGenerations");
}

template <typename T>
template <typename... Arguments>
T *LinkGenerations<T>::join(bool &left_lost, Arguments &&...arguments)
{
    AV_SCOPE("LinkGenerations::join");
    generations_.destroy_if([this](const T &held) { return &held != current_ && held.users() == 0u; });
    left_lost = current_ != nullptr && current_->lost();
    if (left_lost) current_ = nullptr;
    if (current_ != nullptr) return current_;
    if (generations_.create(current_, static_cast<Arguments &&>(arguments)...) != Status::ok) current_ = nullptr;
    return current_;
}

template <typename T>
void LinkGenerations<T>::drop(T &generation)
{
    AV_SCOPE("LinkGenerations::drop");
    if (current_ == &generation) current_ = nullptr;
    static_cast<void>(generations_.destroy(&generation));
}

template <typename T>
uint32_t LinkGenerations<T>::count() const
{
    AV_SCOPE("LinkGenerations::count");
    return generations_.live();
}

template <typename T>
template <typename Visitor>
void LinkGenerations<T>::for_each(Visitor visit) const
{
    AV_SCOPE("LinkGenerations::for_each");
    generations_.for_each([&](const T &held) { visit(held, &held == current_); });
}

template <typename T>
bool LinkGenerations<T>::invariants_hold() const
{
    AV_SCOPE("LinkGenerations::invariants_hold");
    if (!generations_.invariants_hold()) return false;
    return current_ == nullptr || generations_.find(current_) == current_;
}

}

#endif
