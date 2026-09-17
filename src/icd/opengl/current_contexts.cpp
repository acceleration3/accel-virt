#include "icd/opengl/current_contexts.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/thread_slot.hpp"

namespace av::glicd {

CurrentContexts::CurrentContexts(ThreadSlot &slot) : slot_(slot), anywhere_(0u)
{
    AV_SCOPE("CurrentContexts::CurrentContexts");
}

bool CurrentContexts::make_current(GuestGlContext &context)
{
    AV_SCOPE("CurrentContexts::make_current");
    if (current_elsewhere(context)) return false;
    if (current() == &context) return true;
    release();
    context.set_owner(slot_.thread_id());
    slot_.set(&context);
    anywhere_++;
    return true;
}

void CurrentContexts::release()
{
    AV_SCOPE("CurrentContexts::release");
    GuestGlContext *const held = current();
    if (held == nullptr) return;
    held->set_owner(0u);
    slot_.set(nullptr);
    anywhere_--;
}

GuestGlContext *CurrentContexts::current() const
{
    AV_SCOPE("CurrentContexts::current");
    return static_cast<GuestGlContext *>(slot_.get());
}

bool CurrentContexts::current_elsewhere(const GuestGlContext &context) const
{
    AV_SCOPE("CurrentContexts::current_elsewhere");
    return context.owner() != 0u && context.owner() != slot_.thread_id();
}

uint32_t CurrentContexts::anywhere() const
{
    AV_SCOPE("CurrentContexts::anywhere");
    return anywhere_;
}
uint32_t CurrentContexts::thread() const
{
    AV_SCOPE("CurrentContexts::thread");
    return slot_.thread_id();
}

/* This thread's context is current on this thread. */
bool CurrentContexts::invariants_hold() const
{
    AV_SCOPE("CurrentContexts::invariants_hold");
    const GuestGlContext *const held = current();
    if (held == nullptr) return true;
    return anywhere_ != 0u && held->owner() == slot_.thread_id();
}

}  // namespace av::glicd
