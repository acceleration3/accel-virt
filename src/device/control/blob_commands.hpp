/* The virtio-gpu control commands the device core answers instead of the base
 * class: blob creation, mapping and unmapping, and UNREF and CREATE_2D when
 * they name a blob, whose ids share the base class's resource space. What a
 * display head shows is ScanoutCommands'. Every field is checked before memory
 * is mapped or allocated; each answer is one response. Main loop. */
#ifndef AV_BLOB_COMMANDS_HPP
#define AV_BLOB_COMMANDS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class ControlRequest;
class GpuResponse;
class Host;
class WireRecorder;

class BlobCommands {
public:
    enum : uint32_t {   /* VIRTIO_GPU_CMD_* */
        resource_create_2d = 0x0101u,
        resource_unref = 0x0102u,
        resource_create_blob = 0x010Cu,
        resource_map_blob = 0x0208u,
        resource_unmap_blob = 0x0209u,
    };

    BlobCommands(Host &host, BlobTable &blobs, WireRecorder &recorder);
    BlobCommands(const BlobCommands &) = delete;
    BlobCommands &operator=(const BlobCommands &) = delete;

    /* Whether the core answered `request`; false leaves it to the base class. */
    bool answer(ControlRequest &request, uint32_t type);

private:
    static constexpr uint64_t no_context_object = 0u;

    /* The response for a table status. */
    uint32_t create_answer(Status status) const;
    uint32_t create(ControlRequest &request);
    uint32_t create_guest(ControlRequest &request, uint32_t id, uint32_t entries, uint64_t size);
    uint32_t create_host(uint32_t id, uint32_t flags, uint32_t entries, uint64_t size);
    void     map(ControlRequest &request, GpuResponse &response);
    uint32_t unmap(ControlRequest &request);
    /* Whether the request named a blob: a pinned one is refused, not released. */
    bool     unref(ControlRequest &request, GpuResponse &response);
    /* The resource a 32-byte request names, or 0 for a shorter one. */
    uint32_t named_resource(ControlRequest &request);

    Host         &host_;
    BlobTable    &blobs_;
    WireRecorder &recorder_;
};

}  // namespace av

#endif  // AV_BLOB_COMMANDS_HPP
