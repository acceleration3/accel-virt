#include "device/executor/cl_handle_registry.hpp"

namespace av {

namespace {

uint64_t number_of(const void *object)
{
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object));
}

}  // namespace

ClHandleRegistry::ClHandleRegistry() : objects_{}, references_{}, kinds_{}, count_(0u) {}

bool ClHandleRegistry::remember(clproto::HandleKind kind, void *object)
{
    if (object == nullptr) return true;
    const uint64_t number = number_of(object);
    const uint32_t slot = slot_of(number);
    if (slot != count_) return kinds_[slot] == kind;
    if (count_ >= max_handles) return false;
    objects_[count_] = number;
    references_[count_] = 1u;
    kinds_[count_] = kind;
    count_++;
    return true;
}

void *ClHandleRegistry::resolve(clproto::HandleKind kind, uint64_t wire) const
{
    if (wire == 0u) return nullptr;
    const uint32_t slot = slot_of(wire);
    if (slot == count_ || kinds_[slot] != kind) return nullptr;
    return reinterpret_cast<void *>(static_cast<uintptr_t>(wire));
}

void ClHandleRegistry::retain(clproto::HandleKind kind, void *object)
{
    const uint32_t slot = slot_of(number_of(object));
    if (slot == count_ || kinds_[slot] != kind) return;
    /* Saturated rather than wrapped: a count that wrapped to zero would be an
     * entry no release could take out. */
    if (references_[slot] != ~uint32_t{0}) references_[slot]++;
}

void ClHandleRegistry::release(clproto::HandleKind kind, void *object)
{
    const uint32_t slot = slot_of(number_of(object));
    if (slot == count_ || kinds_[slot] != kind) return;
    /* A saturated count no longer says how many the guest holds, so it is
     * never counted down to a forget the ICD may not have done. */
    if (references_[slot] == ~uint32_t{0}) return;
    if (--references_[slot] != 0u) return;
    count_--;
    objects_[slot] = objects_[count_];
    references_[slot] = references_[count_];
    kinds_[slot] = kinds_[count_];
}

void ClHandleRegistry::release_all(clproto::ServerDispatch &icd)
{
    struct Release {
        clproto::HandleKind kind;
        cl_int (*call)(clproto::ServerDispatch &icd, void *object);
    };
    /* Dependents before what they depend on, the order an application tearing
     * down in reverse would use, so the driver is never asked to free a context
     * it still has queues and buffers on. */
    constexpr Release order[] = {
        {clproto::HandleKind::event, [](clproto::ServerDispatch &icd, void *object) {
             return icd.on_clReleaseEvent(static_cast<cl_event>(object)); }},
        {clproto::HandleKind::kernel, [](clproto::ServerDispatch &icd, void *object) {
             return icd.on_clReleaseKernel(static_cast<cl_kernel>(object)); }},
        {clproto::HandleKind::program, [](clproto::ServerDispatch &icd, void *object) {
             return icd.on_clReleaseProgram(static_cast<cl_program>(object)); }},
        {clproto::HandleKind::sampler, [](clproto::ServerDispatch &icd, void *object) {
             return icd.on_clReleaseSampler(static_cast<cl_sampler>(object)); }},
        {clproto::HandleKind::mem, [](clproto::ServerDispatch &icd, void *object) {
             return icd.on_clReleaseMemObject(static_cast<cl_mem>(object)); }},
        {clproto::HandleKind::command_queue, [](clproto::ServerDispatch &icd, void *object) {
             return icd.on_clReleaseCommandQueue(static_cast<cl_command_queue>(object)); }},
        {clproto::HandleKind::context, [](clproto::ServerDispatch &icd, void *object) {
             return icd.on_clReleaseContext(static_cast<cl_context>(object)); }},
        {clproto::HandleKind::device, [](clproto::ServerDispatch &icd, void *object) {
             return icd.on_clReleaseDevice(static_cast<cl_device_id>(object)); }},
    };
    for (const Release &release : order) {
        for (uint32_t slot = 0u; slot < count_; slot++) {
            if (kinds_[slot] != release.kind) continue;
            const uint32_t times = references_[slot] == ~uint32_t{0} ? 1u : references_[slot];
            void *const object = reinterpret_cast<void *>(static_cast<uintptr_t>(objects_[slot]));
            /* What the ICD answers changes nothing: the context is gone either
             * way, and there is nobody left to tell. */
            for (uint32_t n = 0u; n < times; n++) (void)release.call(icd, object);
        }
    }
    forget_all();
}

void ClHandleRegistry::forget_all() { count_ = 0u; }

uint32_t ClHandleRegistry::count() const { return count_; }

uint32_t ClHandleRegistry::slot_of(uint64_t object) const
{
    for (uint32_t i = 0u; i < count_; i++) {
        if (objects_[i] == object) return i;
    }
    return count_;
}

bool ClHandleRegistry::invariants_hold() const
{
    if (count_ > max_handles) return false;
    for (uint32_t i = 0u; i < count_; i++) {
        if (objects_[i] == 0u || references_[i] == 0u) return false;
    }
    return true;
}

}  // namespace av
