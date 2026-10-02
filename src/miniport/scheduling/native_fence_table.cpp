#include "miniport/scheduling/native_fence_table.hpp"

#include "miniport/ddi/dxgkarg_native_fence_records.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/platform/mutex_try_guard.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

NativeFenceTable::NativeFenceTable(Platform &platform)
    : platform_(platform), lock_(platform, LockRank::native_fence_table), live_slots_{}, types_{}, monitored_{},
      current_{}, live_(0u)
{
    AV_SCOPE("NativeFenceTable::NativeFenceTable");
}

void *NativeFenceTable::handle_of(uint32_t slot) const
{
    AV_SCOPE("NativeFenceTable::handle_of");
    return reinterpret_cast<void *>(static_cast<uintptr_t>(slot) + 1u);
}

uint32_t NativeFenceTable::slot_of(const void *handle) const
{
    AV_SCOPE("NativeFenceTable::slot_of");
    const uintptr_t named = reinterpret_cast<uintptr_t>(handle);
    if (named == 0u || named > max_fences) return max_fences;
    const uint32_t slot = static_cast<uint32_t>(named - 1u);
    return live_slots_[slot] ? slot : max_fences;
}

Status NativeFenceTable::create(DxgkargCreateNativeFenceRecord &request)
{
    AV_SCOPE("NativeFenceTable::create");
    platform_.require_irql_at_most(Irql::passive, "NativeFenceTable::create");
    MutexGuard                       lock(lock_);
    InvariantScope<NativeFenceTable> guard(platform_, *this, "create");
    /* A fence type this driver has no meaning for is one whose semantics it
     * would be guessing at (constitution V). */
    if (request.Type != native_fence_type_default && request.Type != native_fence_type_intra_gpu)
        return Status::invalid_parameter;
    for (uint32_t slot = 0u; slot < max_fences; slot++) {
        if (live_slots_[slot]) continue;
        live_slots_[slot] = true;
        types_[slot] = request.Type;
        monitored_[slot] = 0u;
        current_[slot] = 0u;
        live_++;
        request.hGlobalNativeFence = handle_of(slot);
        return Status::ok;
    }
    return Status::no_space;
}

Status NativeFenceTable::destroy(const DxgkargDestroyNativeFenceRecord &request)
{
    AV_SCOPE("NativeFenceTable::destroy");
    platform_.require_irql_at_most(Irql::passive, "NativeFenceTable::destroy");
    MutexGuard                       lock(lock_);
    InvariantScope<NativeFenceTable> guard(platform_, *this, "destroy");
    const uint32_t slot = slot_of(request.hGlobalNativeFence);
    if (slot == max_fences) return Status::invalid_parameter;
    live_slots_[slot] = false;
    types_[slot] = 0u;
    monitored_[slot] = 0u;
    current_[slot] = 0u;
    live_--;
    return Status::ok;
}

/* The OS names every fence and every address, and this driver writes what it
 * asked for. A request that names anything this table did not make is refused
 * whole: half-written values would leave the OS's waiters reading a mixture of
 * what it asked for and what it did not. */
Status NativeFenceTable::update(const DxgkargUpdateNativeFenceValuesRecord &request,
                                uint64_t (&values)[max_fences])
{
    AV_SCOPE("NativeFenceTable::update");
    /* No lock: the monitored values arrive as high as PROFILE, where nothing
     * may be taken, and nothing needs one - the slots are fixed storage, the
     * OS owns the memory being written, and a fence it is destroying is one it
     * is not updating. */
    if (request.NativeFenceArray == nullptr || request.UpdatedValueArray == nullptr ||
        request.ValueKernelCpuVa == nullptr || request.NumFences == 0u || request.NumFences > max_fences)
        return Status::invalid_parameter;
    for (uint32_t index = 0u; index < request.NumFences; index++)
        if (slot_of(request.NativeFenceArray[index]) == max_fences ||
            request.ValueKernelCpuVa[index] == nullptr)
            return Status::invalid_parameter;
    for (uint32_t index = 0u; index < request.NumFences; index++) {
        const uint32_t slot = slot_of(request.NativeFenceArray[index]);
        platform_.store_atomic(values[slot], request.UpdatedValueArray[index]);
        platform_.store_atomic(*static_cast<uint64_t *>(request.ValueKernelCpuVa[index]),
                               request.UpdatedValueArray[index]);
    }
    return Status::ok;
}

Status NativeFenceTable::update_monitored(const DxgkargUpdateNativeFenceValuesRecord &request)
{
    AV_SCOPE("NativeFenceTable::update_monitored");
    platform_.require_irql_at_most(Irql::isr, "NativeFenceTable::update_monitored");
    return update(request, monitored_);
}

Status NativeFenceTable::update_current(const DxgkargUpdateNativeFenceValuesRecord &request)
{
    AV_SCOPE("NativeFenceTable::update_current");
    platform_.require_irql_at_most(Irql::dispatch, "NativeFenceTable::update_current");
    return update(request, current_);
}

uint32_t NativeFenceTable::live() const
{
    AV_SCOPE("NativeFenceTable::live");
    return live_;
}

uint64_t NativeFenceTable::monitored_value(const void *handle) const
{
    AV_SCOPE("NativeFenceTable::monitored_value");
    const uint32_t slot = slot_of(handle);
    return slot != max_fences ? monitored_[slot] : 0u;
}

uint64_t NativeFenceTable::current_value(const void *handle) const
{
    AV_SCOPE("NativeFenceTable::current_value");
    const uint32_t slot = slot_of(handle);
    return slot != max_fences ? current_[slot] : 0u;
}

void NativeFenceTable::report(StateWriter &writer)
{
    AV_SCOPE("NativeFenceTable::report");
    platform_.require_irql_at_most(Irql::apc, "NativeFenceTable::report");
    const MutexTryGuard guard(lock_);
    writer.open("native_fence_table", 0u);
    writer.flag("busy", !guard.held());
    if (guard.held()) {
        writer.number("live", live_);
        for (uint32_t slot = 0u; slot < max_fences; slot++) {
            if (!live_slots_[slot]) continue;
            writer.open("native_fence", slot);
            writer.number("type", types_[slot]);
            writer.number("current", current_[slot]);
            writer.number("monitored", monitored_[slot]);
            writer.close();
        }
    }
    writer.close();
}

bool NativeFenceTable::invariants_hold() const
{
    AV_SCOPE("NativeFenceTable::invariants_hold");
    uint32_t counted = 0u;
    for (uint32_t slot = 0u; slot < max_fences; slot++) {
        if (live_slots_[slot]) {
            counted++;
            continue;
        }
        /* A fence nobody made holds nothing. */
        if (current_[slot] != 0u || monitored_[slot] != 0u) return false;
    }
    return counted == live_;
}

}  // namespace av
