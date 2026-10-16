#include "executor/opencl/cl_handle_registry.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

uint64_t number_of(const void *object)
{
    AV_SCOPE("number_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object));
}

}  // namespace

ClHandleRegistry::ClHandleRegistry(BlockSource &blocks) : handles_(blocks, Pool::paged)
{
    AV_SCOPE("ClHandleRegistry::ClHandleRegistry");
}

Status ClHandleRegistry::make_room(uint64_t objects)
{
    AV_SCOPE("ClHandleRegistry::make_room");
    const uint64_t wanted = uint64_t{handles_.count()} + objects;
    if (wanted > KeyedSlots<uint64_t, ClHandle>::most_entries) return Status::no_memory;
    return handles_.reserve(static_cast<uint32_t>(wanted));
}

Status ClHandleRegistry::remember(clproto::HandleKind kind, void *object)
{
    AV_SCOPE("ClHandleRegistry::remember");
    if (object == nullptr) return Status::ok;
    const ClHandle *const known = handles_.find(number_of(object));
    if (known != nullptr) return known->kind_ == kind ? Status::ok : Status::conflict;
    uint32_t slot = 0u;
    return handles_.insert(number_of(object), ClHandle{1u, kind}, slot);
}

ClHandle *ClHandleRegistry::held(clproto::HandleKind kind, uint64_t object)
{
    AV_SCOPE("ClHandleRegistry::held");
    ClHandle *const found = object != 0u ? handles_.find(object) : nullptr;
    return found != nullptr && found->kind_ == kind ? found : nullptr;
}

void *ClHandleRegistry::resolve(clproto::HandleKind kind, uint64_t wire) const
{
    AV_SCOPE("ClHandleRegistry::resolve");
    const ClHandle *const found = wire != 0u ? handles_.find(wire) : nullptr;
    return found != nullptr && found->kind_ == kind ? reinterpret_cast<void *>(static_cast<uintptr_t>(wire)) : nullptr;
}

void ClHandleRegistry::retain(clproto::HandleKind kind, void *object)
{
    AV_SCOPE("ClHandleRegistry::retain");
    ClHandle *const found = held(kind, number_of(object));
    /* Saturated rather than wrapped: a count that wrapped to zero would be an
     * entry no release could take out. */
    if (found != nullptr && found->references_ != ~uint32_t{0}) found->references_++;
}

void ClHandleRegistry::release(clproto::HandleKind kind, void *object)
{
    AV_SCOPE("ClHandleRegistry::release");
    ClHandle *const found = held(kind, number_of(object));
    /* A saturated count no longer says how many the guest holds, so it is
     * never counted down to a forget the ICD may not have done. */
    if (found == nullptr || found->references_ == ~uint32_t{0}) return;
    if (--found->references_ == 0u) handles_.erase(number_of(object));
}

void ClHandleRegistry::release_all(clproto::ServerDispatch &icd)
{
    AV_SCOPE("ClHandleRegistry::release_all");
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
        handles_.for_each([&release, &icd](uint64_t number, const ClHandle &handle) {
            if (handle.kind_ != release.kind) return;
            const uint32_t times = handle.references_ == ~uint32_t{0} ? 1u : handle.references_;
            void *const    object = reinterpret_cast<void *>(static_cast<uintptr_t>(number));
            /* What the ICD answers changes nothing: the context is gone either
             * way, and there is nobody left to tell. */
            for (uint32_t n = 0u; n < times; n++) (void)release.call(icd, object);
        });
    }
    forget_all();
}

void ClHandleRegistry::forget_all()
{
    AV_SCOPE("ClHandleRegistry::forget_all");
    handles_.clear();
}

uint32_t ClHandleRegistry::count() const
{
    AV_SCOPE("ClHandleRegistry::count");
    return handles_.count();
}

bool ClHandleRegistry::invariants_hold() const
{
    AV_SCOPE("ClHandleRegistry::invariants_hold");
    bool holds = true;
    handles_.for_each([&holds](uint64_t number, const ClHandle &handle) {
        holds = holds && number != 0u && handle.references_ != 0u;
    });
    return holds;
}

}  // namespace av
