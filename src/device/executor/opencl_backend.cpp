#include "device/executor/opencl_backend.hpp"

#include "device/abi/rvk_executor.h"
#include "device/executor/imported_blob.hpp"
#include "proto/generated/opencl/cl_command_names.h"
#include "proto/generated/opencl/cl_dispatch_server.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <string.h>

namespace av {

OpenClBackend::OpenClBackend() : handles_(), ran_(0u), lost_(false) {}

bool OpenClBackend::knows(uint32_t command_id) const
{
    if (command_id >> 16u != opencl_api) return false;
    /* The generator writes one name per id it carries. An id the profile does
     * not carry - clGetKernelSuggestedLocalWorkSize is the only one reserved
     * today - is a skip, not a fault: a guest using a command this build left
     * out must not have its context stopped for it. */
    return strcmp(clproto::cmd_id_to_string(command_id), "unknown") != 0;
}

bool OpenClBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                        WireArena &arena)
{
    if (!clproto::dispatch_command(*this, command_id, payload, reply, arena)) return false;
    ran_++;
    return true;
}

bool OpenClBackend::lost() const { return lost_; }

bool OpenClBackend::adopt(ImportedBlob &)
{
    /* Nothing yet. Importing here means clCreateBufferWithProperties with a
     * dma-buf, and whether that works is a per-device question this host
     * answers differently on each of its three OpenCL platforms - so it is
     * asked where the answer is used, not guessed here. Until then a blob is
     * copied at the boundary, which is what import_kinds already says. */
    return false;
}

void OpenClBackend::abandon(ImportedBlob &) {}

void *OpenClBackend::handle_for(clproto::HandleKind kind, uint64_t wire) const
{
    return handles_.resolve(kind, wire);
}

void OpenClBackend::made(clproto::HandleKind kind, void *object)
{
    /* A registry with no room loses the device rather than hand the guest a
     * number this host would refuse when it came back. */
    if (!handles_.remember(kind, object)) lost_ = true;
}

void OpenClBackend::retained(clproto::HandleKind kind, void *object) { handles_.retain(kind, object); }

void OpenClBackend::released(clproto::HandleKind kind, void *object) { handles_.release(kind, object); }

void OpenClBackend::shut_down()
{
    handles_.forget_all();
    ran_ = 0u;
    lost_ = false;
}

uint64_t OpenClBackend::ran() const { return ran_; }

}  // namespace av
