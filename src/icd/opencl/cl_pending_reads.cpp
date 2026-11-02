#include "icd/opencl/cl_pending_reads.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClPendingReads::ClPendingReads() : rows_(), held_(0u), delivered_(0u)
{
    AV_SCOPE("ClPendingReads::ClPendingReads");
}

bool ClPendingReads::record(uint64_t event, uint64_t queue, uint64_t buffer, uint64_t offset,
                            uint64_t bytes, void *into, bool ours)
{
    AV_SCOPE("ClPendingReads::record");
    uint32_t slot = 0u;
    while (slot < max_reads && rows_[slot].held()) ++slot;
    if (slot == max_reads) return false;
    if (!rows_[slot].take(event, queue, buffer, static_cast<size_t>(offset),
                          static_cast<size_t>(bytes), into, ours))
        return false;
    ++held_;
    return true;
}

ClPendingRead *ClPendingReads::waiting_on(uint64_t event)
{
    AV_SCOPE("ClPendingReads::waiting_on");
    if (event == 0u) return nullptr;
    for (uint32_t slot = 0u; slot < max_reads; ++slot) {
        if (rows_[slot].held() && rows_[slot].event() == event) return &rows_[slot];
    }
    return nullptr;
}

const ClPendingRead *ClPendingReads::at(uint32_t slot) const
{
    AV_SCOPE("ClPendingReads::at");
    return slot < max_reads ? &rows_[slot] : &rows_[0];
}

uint32_t ClPendingReads::forget_queue(uint64_t queue, uint64_t *markers)
{
    AV_SCOPE("ClPendingReads::forget_queue");
    if (queue == 0u) return 0u;
    uint32_t taken = 0u;
    for (uint32_t slot = 0u; slot < max_reads; ++slot) {
        if (!rows_[slot].held() || rows_[slot].queue() != queue) continue;
        if (rows_[slot].ours() && markers != nullptr) markers[taken++] = rows_[slot].event();
        rows_[slot].give_up();
        --held_;
    }
    return taken;
}

uint32_t ClPendingReads::outstanding() const
{
    AV_SCOPE("ClPendingReads::outstanding");
    return held_;
}

uint64_t ClPendingReads::delivered() const
{
    AV_SCOPE("ClPendingReads::delivered");
    return delivered_;
}

void ClPendingReads::count_delivery()
{
    AV_SCOPE("ClPendingReads::count_delivery");
    ++delivered_;
    --held_;
}

bool ClPendingReads::invariants_hold() const
{
    AV_SCOPE("ClPendingReads::invariants_hold");
    if (held_ > max_reads) return false;
    uint32_t held = 0u;
    for (uint32_t slot = 0u; slot < max_reads; ++slot) {
        if (!rows_[slot].invariants_hold()) return false;
        if (rows_[slot].held()) ++held;
    }
    return held == held_;
}

}
