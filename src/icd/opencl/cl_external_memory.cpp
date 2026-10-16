#include "icd/opencl/cl_external_memory.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/log/log_sink.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "proto/resources/shared_object_kind.hpp"

#include <CL/cl_ext.h>

namespace av::clicd {

/* The header writes the five property names out as numbers so that nothing
 * above it has to include cl_ext.h to judge a list; these hold it to what the
 * registry says they are. */
static_assert(ClExternalMemory::handle_properties[0] == CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR);
static_assert(ClExternalMemory::handle_properties[1] == CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KMT_KHR);
static_assert(ClExternalMemory::handle_properties[2] == CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_NAME_KHR);
static_assert(ClExternalMemory::handle_properties[3] == CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR);
static_assert(ClExternalMemory::handle_properties[4] == CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR);

ClExternalMemory::ClExternalMemory(icd::SharedHandles &handles, BlockSource &memory, LogSink &log)
    : log_(log), handles_(handles), opened_(handles, memory, log, "avcl: ")
{
    AV_SCOPE("ClExternalMemory::ClExternalMemory");
}

bool ClExternalMemory::names_handle(const ClPropertyList &list, uint64_t &handle, bool &taken) const
{
    AV_SCOPE("ClExternalMemory::names_handle");
    handle = 0u;
    taken = false;
    uint32_t found = 0u;
    for (const uint64_t property : handle_properties) {
        uint64_t value = 0u;
        if (!list.names(property, value)) continue;
        if (found == 0u) {
            handle = value;
            taken = property == handle_properties[0];
        }
        found++;
    }
    /* Two handle types name two different objects, and there is one buffer:
     * a list that asks for both is refused rather than answered with whichever
     * this loop met first. */
    if (found > 1u) taken = false;
    return found != 0u;
}

bool ClExternalMemory::asked_for(const ClPropertyList &list) const
{
    AV_SCOPE("ClExternalMemory::asked_for");
    uint64_t handle = 0u;
    bool     taken = false;
    return names_handle(list, handle, taken);
}

cl_int ClExternalMemory::open(ClientTransport &wire, uint32_t device, const ClPropertyList &list,
                              uint64_t size, uint64_t &made)
{
    AV_SCOPE("ClExternalMemory::open");
    made = 0u;
    uint64_t named = 0u;
    bool     taken = false;
    if (!names_handle(list, named, taken)) return CL_INVALID_VALUE;
    if (!taken) {
        log_.write("avcl: an external memory handle type this driver does not take, or two at once");
        return CL_INVALID_PROPERTY;
    }
    return answer(opened_.open(wire, device, named, SharedObjectKind::memory, size, 0u, made));
}

cl_int ClExternalMemory::open_own(ClientTransport &wire, uint32_t device, uint64_t handle, uint64_t size,
                                  uint64_t &made, bool &closed)
{
    AV_SCOPE("ClExternalMemory::open_own");
    const cl_int answered = answer(opened_.open(wire, device, handle, SharedObjectKind::memory, size, 0u, made));
    uint64_t     object_id = 0u;
    closed = handles_.resolve(handle, object_id);
    if (closed) handles_.forget(handle);
    return answered;
}

cl_int ClExternalMemory::answer(Status opened) const
{
    AV_SCOPE("ClExternalMemory::answer");
    if (opened == Status::no_memory) return CL_OUT_OF_HOST_MEMORY;
    if (opened == Status::invalid_state) return CL_OUT_OF_RESOURCES;
    return opened == Status::ok ? CL_SUCCESS : CL_INVALID_VALUE;
}

bool ClExternalMemory::answered(uint64_t made) const
{
    AV_SCOPE("ClExternalMemory::answered");
    return opened_.answered(SharedObjectKind::memory, made);
}

bool ClExternalMemory::give_back(ClientTransport &wire, uint64_t made)
{
    AV_SCOPE("ClExternalMemory::give_back");
    return opened_.give_back(wire, SharedObjectKind::memory, made);
}

void ClExternalMemory::release_all(ClientTransport &wire)
{
    AV_SCOPE("ClExternalMemory::release_all");
    opened_.release_all(wire);
}

uint32_t ClExternalMemory::live() const
{
    AV_SCOPE("ClExternalMemory::live");
    return opened_.live();
}

void ClExternalMemory::report(StateWriter &writer) const
{
    AV_SCOPE("ClExternalMemory::report");
    writer.open("external_memory", 0u);
    writer.number("live", opened_.live());
    opened_.report(writer);
    writer.close();
}

bool ClExternalMemory::invariants_hold() const
{
    AV_SCOPE("ClExternalMemory::invariants_hold");
    return opened_.invariants_hold();
}

}  // namespace av::clicd
