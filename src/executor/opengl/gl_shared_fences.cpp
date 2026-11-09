#include "executor/opengl/gl_shared_fences.hpp"

#include "core/trace/function_scope.hpp"

#include <time.h>

namespace av {

GlSharedFences::GlSharedFences() : lock_(), entries_{}, next_(0u), count_(0u), newest_(0u)
{
    AV_SCOPE("GlSharedFences::GlSharedFences");
}

void GlSharedFences::publish(uint64_t token, void *fence)
{
    AV_SCOPE("GlSharedFences::publish");
    if (token == 0u || fence == nullptr) return;
    lock_.acquire();
    entries_[next_] = Entry{token, fence};
    next_ = (next_ + 1u) % most_fences;
    if (count_ < most_fences) count_++;
    if (token > newest_) newest_ = token;
    lock_.release();
}

void GlSharedFences::withdraw(void *fence)
{
    AV_SCOPE("GlSharedFences::withdraw");
    if (fence == nullptr) return;
    lock_.acquire();
    for (Entry &entry : entries_) {
        if (entry.fence_ != fence) continue;
        entry = Entry{0u, nullptr};
        if (count_ != 0u) count_--;
    }
    lock_.release();
}

void *GlSharedFences::find(uint64_t token)
{
    AV_SCOPE("GlSharedFences::find");
    if (token == 0u) return nullptr;
    void *found = nullptr;
    lock_.acquire();
    for (const Entry &entry : entries_)
        if (entry.token_ == token) found = entry.fence_;
    lock_.release();
    return found;
}

void *GlSharedFences::await(uint64_t token, uint64_t nanoseconds)
{
    AV_SCOPE("GlSharedFences::await");
    constexpr uint64_t step = 200000u;
    uint64_t           waited = 0u;
    for (;;) {
        void *const found = find(token);
        if (found != nullptr || waited >= nanoseconds) return found;
        lock_.acquire();
        const bool already_past = token <= newest_;
        lock_.release();
        if (already_past) return nullptr;
        const timespec pause{0, static_cast<long>(step)};
        nanosleep(&pause, nullptr);
        waited += step;
    }
}

uint32_t GlSharedFences::count()
{
    AV_SCOPE("GlSharedFences::count");
    lock_.acquire();
    const uint32_t held = count_;
    lock_.release();
    return held;
}

bool GlSharedFences::invariants_hold()
{
    AV_SCOPE("GlSharedFences::invariants_hold");
    lock_.acquire();
    uint32_t live = 0u;
    bool     sound = true;
    for (const Entry &entry : entries_) {
        if ((entry.token_ == 0u) != (entry.fence_ == nullptr)) sound = false;
        if (entry.fence_ != nullptr) live++;
    }
    sound = sound && live == count_ && next_ < most_fences;
    lock_.release();
    return sound;
}

}
