#include "executor/opengl/gl_shared_fences.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GlSharedFences::GlSharedFences() : lock_(), entries_{}, next_(0u), count_(0u)
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
