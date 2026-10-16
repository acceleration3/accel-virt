/* The virtio-gpu control commands the device core answers instead of the base
 * class: blob creation, mapping and unmapping, and UNREF, CREATE_2D and
 * ATTACH_BACKING when they name a blob, whose ids share the base class's
 * resource space. A GUEST blob is guest pages the device reads itself; a
 * HOST3D blob is host GPU memory, made for one context and the only kind that
 * is mapped (docs/host-memory.md); a HOST3D_GUEST blob is host GPU memory laid
 * over guest pages a process lent, which ATTACH_BACKING names a piece at a
 * time ("Application memory"). What a display head shows is ScanoutCommands'. Every
 * field is checked before memory is mapped or allocated; each answer is one
 * response. Main loop. */
#ifndef AV_BLOB_COMMANDS_HPP
#define AV_BLOB_COMMANDS_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobTable;
class ContextPlane;
class CreateBlobRequest;
class HostBlobTable;
class ControlRequest;
class GpuResponse;
class Host;
class WireRecorder;

class BlobCommands {
public:
    enum : uint32_t {   /* VIRTIO_GPU_CMD_* */
        resource_create_2d = 0x0101u,
        resource_unref = 0x0102u,
        resource_attach_backing = 0x0106u,
        resource_create_blob = 0x010Cu,
        resource_map_blob = 0x0208u,
        resource_unmap_blob = 0x0209u,
    };

    /* `contexts` is told of every guest blob made and let go, for contexts
     * that keep a copy of the table in another process. */
    BlobCommands(Host &host, BlobTable &blobs, HostBlobTable &host_blobs, WireRecorder &recorder,
                 ContextPlane &contexts);
    BlobCommands(const BlobCommands &) = delete;
    BlobCommands &operator=(const BlobCommands &) = delete;

    /* Whether the core answered `request`; false leaves it to the base class. */
    bool answer(ControlRequest &request, uint32_t type);

private:
    static constexpr uint64_t no_context_object = 0u;
    /* A host blob's context object is a HostMemoryRequest, 32 bits wide. */
    static constexpr uint64_t request_mask = 0xFFFFFFFFu;
    /* RESOURCE_ATTACH_BACKING: the header, the resource and the entry count,
     * then the entries. */
    static constexpr size_t   attach_header_size = 32u;

    /* The response for a table status. */
    uint32_t create_answer(Status status) const;
    uint32_t create(ControlRequest &request);
    uint32_t create_guest(ControlRequest &request, uint32_t id, uint32_t entries, uint64_t size);
    uint32_t create_host(const CreateBlobRequest &create);
    uint32_t create_adopted(const CreateBlobRequest &create);
    /* Whether the request named a host blob: its pieces laid over lent pages. */
    bool     attach(ControlRequest &request, GpuResponse &response);
    void     map(ControlRequest &request, GpuResponse &response);
    uint32_t unmap(ControlRequest &request);
    /* Whether the request named a blob: a pinned one is refused, not released. */
    bool     unref(ControlRequest &request, GpuResponse &response);
    /* The resource a 32-byte request names, or 0 for a shorter one. */
    uint32_t named_resource(ControlRequest &request);

    Host         &host_;
    BlobTable    &blobs_;
    HostBlobTable &host_blobs_;
    WireRecorder &recorder_;
    ContextPlane &contexts_;
};

}  // namespace av

#endif  // AV_BLOB_COMMANDS_HPP
