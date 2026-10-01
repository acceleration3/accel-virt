#include "icd/opencl/cl_external_memory.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/shared_object_call.hpp"
#include "core/support/log_sink.hpp"
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

ClExternalMemory::ClExternalMemory(icd::SharedHandles &handles, LogSink &log)
    : handles_(handles), log_(log), rows_{}, live_(0u)
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

ClImportedMemoryRow *ClExternalMemory::row_of(uint64_t made)
{
    AV_SCOPE("ClExternalMemory::row_of");
    if (made == 0u) return nullptr;
    for (uint32_t index = 0u; index < live_; index++)
        if (rows_[index].made_ == made) return &rows_[index];
    return nullptr;
}

const ClImportedMemoryRow *ClExternalMemory::row_of(uint64_t made) const
{
    AV_SCOPE("ClExternalMemory::row_of");
    if (made == 0u) return nullptr;
    for (uint32_t index = 0u; index < live_; index++)
        if (rows_[index].made_ == made) return &rows_[index];
    return nullptr;
}

bool ClExternalMemory::holds(uint64_t object_id) const
{
    AV_SCOPE("ClExternalMemory::holds");
    for (uint32_t index = 0u; index < live_; index++)
        if (rows_[index].object_id_ == object_id) return true;
    return false;
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
    if (named == 0u) return CL_INVALID_VALUE;
    /* Rounded before it is judged, and refused where rounding would wrap: the
     * publisher's record names whole pages and an open is compared to it. */
    if (size == 0u || size > ~uint64_t{0} - (page_bytes - 1u)) return CL_INVALID_VALUE;
    const uint64_t asked = (size + page_bytes - 1u) & ~(page_bytes - 1u);
    if (live_ == max_objects) return CL_OUT_OF_RESOURCES;

    uint64_t object_id = 0u;
    if (!handles_.resolve(named, object_id)) {
        /* Not one this process exported: the kernel is asked whether it may
         * open it, which is the only thing that tells a handle another process
         * sent from a number an application invented. */
        const Status adopted = handles_.adopt(device, named, object_id);
        if (adopted != Status::ok) {
            log_.write("avcl: a Win32 handle this process may not open");
            return CL_INVALID_VALUE;
        }
    }
    if (holds(object_id)) {
        log_.write("avcl: an object this context already imported");
        return CL_INVALID_VALUE;
    }
    SharedObjectCall call(wire);
    uint64_t         opened = 0u;
    /* An open that answered no handle is malformed to the call itself - an
     * ok carrying nothing is an answer no guest could use - so there is
     * nothing left here to check for. */
    if (call.open(object_id, SharedObjectKind::memory, asked, opened) != Status::ok) {
        log_.write("avcl: the device would not open a shared object for this context");
        return CL_OUT_OF_RESOURCES;
    }
    rows_[live_] = ClImportedMemoryRow{object_id, opened, asked};
    live_++;
    made = opened;
    return CL_SUCCESS;
}

bool ClExternalMemory::answered(uint64_t made) const
{
    AV_SCOPE("ClExternalMemory::answered");
    return row_of(made) != nullptr;
}

bool ClExternalMemory::give_back(ClientTransport &wire, uint64_t made)
{
    AV_SCOPE("ClExternalMemory::give_back");
    ClImportedMemoryRow *const row = row_of(made);
    if (row == nullptr) return false;
    SharedObjectCall call(wire);
    static_cast<void>(call.release(row->object_id_, SharedObjectKind::memory, row->size_));
    live_--;
    *row = rows_[live_];
    return true;
}

void ClExternalMemory::release_all(ClientTransport &wire)
{
    AV_SCOPE("ClExternalMemory::release_all");
    while (live_ != 0u) static_cast<void>(give_back(wire, rows_[live_ - 1u].made_));
}

uint32_t ClExternalMemory::live() const
{
    AV_SCOPE("ClExternalMemory::live");
    return live_;
}

void ClExternalMemory::report(StateWriter &writer) const
{
    AV_SCOPE("ClExternalMemory::report");
    writer.open("external_memory", 0u);
    writer.number("live", live_);
    for (uint32_t index = 0u; index < live_; index++) {
        writer.open("object", rows_[index].object_id_);
        writer.number("mem", rows_[index].made_);
        writer.number("size", rows_[index].size_);
        writer.close();
    }
    writer.close();
}

bool ClExternalMemory::invariants_hold() const
{
    AV_SCOPE("ClExternalMemory::invariants_hold");
    if (live_ > max_objects) return false;
    for (uint32_t index = 0u; index < live_; index++) {
        if (rows_[index].object_id_ == 0u || rows_[index].made_ == 0u || rows_[index].size_ == 0u)
            return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (rows_[earlier].object_id_ == rows_[index].object_id_ ||
                rows_[earlier].made_ == rows_[index].made_)
                return false;
    }
    return true;
}

}  // namespace av::clicd
