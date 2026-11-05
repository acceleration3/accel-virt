#include "icd/opengl/queued_forgets.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

QueuedForgets::QueuedForgets(BlockSource &memory) : buffers_(memory, Pool::paged), waiting_(0u)
{
    AV_SCOPE("QueuedForgets::QueuedForgets");
}

Status QueuedForgets::add(uint32_t buffer)
{
    AV_SCOPE("QueuedForgets::add");
    const Status appended = buffers_.append(buffer);
    if (appended != Status::ok) return appended;
    __atomic_store_n(&waiting_, buffers_.size(), __ATOMIC_RELEASE);
    return Status::ok;
}

bool QueuedForgets::take(uint32_t &buffer)
{
    AV_SCOPE("QueuedForgets::take");
    if (buffers_.size() == 0u) return false;
    const uint32_t last = buffers_.size() - 1u;
    buffer = buffers_.at(last);
    buffers_.truncate(last);
    __atomic_store_n(&waiting_, last, __ATOMIC_RELEASE);
    return true;
}

uint32_t QueuedForgets::waiting() const
{
    AV_SCOPE("QueuedForgets::waiting");
    return __atomic_load_n(&waiting_, __ATOMIC_ACQUIRE);
}

void QueuedForgets::clear()
{
    AV_SCOPE("QueuedForgets::clear");
    buffers_.clear();
    __atomic_store_n(&waiting_, 0u, __ATOMIC_RELEASE);
}

bool QueuedForgets::invariants_hold() const
{
    AV_SCOPE("QueuedForgets::invariants_hold");
    return waiting_ == buffers_.size() && buffers_.invariants_hold();
}

}
