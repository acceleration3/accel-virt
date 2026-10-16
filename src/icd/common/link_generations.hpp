/* The T new users join, and the lost Ts kept for their remaining users.
 *
 * What a driver's objects share on an adapter - the device OpenGL's and
 * OpenCL's contexts are made on (DeviceLink), a Direct3D runtime's session
 * (D3dSession) - is one generation at a time: every new user joins the
 * current one. When the host loses it, it stays its users' until the last
 * lets go, and the next user to come gets a fresh one beside it, however
 * many lost ones are still held. A newcomer cannot wait for the lost one to
 * go: DWM makes its new device before it lets the removed one go, and on the
 * lost session that device would fail to be made, and the desktop with it.
 *
 * A T says how many users hold it (users()) and whether the host lost it
 * (lost()). One that no user holds and newcomers no longer join is
 * destroyed at the next join. The current one with no users stays, for the
 * next user to open again. Vulkan keeps none: its context is its instance's
 * alone, and the host's handles are scoped to it.
 *
 * Its owner serializes it. */
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

    /* The T a new user joins: the current one, or - when there is none, or
     * the host lost it - a new one made from `arguments`, which becomes
     * current; `left_lost` says a lost one was left to its users. Null, and
     * nothing current, when there is no memory for one. */
    template <typename... Arguments>
    T   *join(bool &left_lost, Arguments &&...arguments);
    /* `generation` destroyed now: its last user let go and nothing is to
     * open it again. */
    void drop(T &generation);
    uint32_t count() const;
    /* Every generation, in no order, and whether it is the current one. */
    template <typename Visitor>
    void for_each(Visitor visit) const;

    /* The current generation is one of them. */
    bool invariants_hold() const;

private:
    ObjectPool<T> generations_;
    /* What new users join; null until one is asked for, after a drop of it,
     * and once the host lost it. */
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

}  // namespace av::icd

#endif  // AV_ICD_LINK_GENERATIONS_HPP
