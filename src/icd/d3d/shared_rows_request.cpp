#include "icd/d3d/shared_rows_request.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

SharedRowsRequest::SharedRowsRequest() : threads_{}
{
    AV_SCOPE("SharedRowsRequest::SharedRowsRequest");
}

bool SharedRowsRequest::ask(uint32_t thread)
{
    AV_SCOPE("SharedRowsRequest::ask");
    if (thread == 0u) return false;
    if (slot_of(thread) != most_threads) return true;
    for (uint32_t &slot : threads_) {
        uint32_t empty = 0u;
        if (__atomic_compare_exchange_n(&slot, &empty, thread, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return true;
    }
    return false;
}

void SharedRowsRequest::withdraw(uint32_t thread)
{
    AV_SCOPE("SharedRowsRequest::withdraw");
    const uint32_t slot = slot_of(thread);
    if (thread != 0u && slot != most_threads) __atomic_store_n(&threads_[slot], 0u, __ATOMIC_RELEASE);
}

bool SharedRowsRequest::asked_by(uint32_t thread) const
{
    AV_SCOPE("SharedRowsRequest::asked_by");
    return thread != 0u && slot_of(thread) != most_threads;
}

uint32_t SharedRowsRequest::slot_of(uint32_t thread) const
{
    AV_SCOPE("SharedRowsRequest::slot_of");
    for (uint32_t slot = 0u; slot < most_threads; slot++)
        if (__atomic_load_n(&threads_[slot], __ATOMIC_ACQUIRE) == thread) return slot;
    return most_threads;
}

}  // namespace av::d3d
