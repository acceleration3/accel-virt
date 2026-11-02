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
    enum : uint32_t {
        resource_create_2d = 0x0101u,
        resource_unref = 0x0102u,
        resource_attach_backing = 0x0106u,
        resource_create_blob = 0x010Cu,
        resource_map_blob = 0x0208u,
        resource_unmap_blob = 0x0209u,
    };

    BlobCommands(Host &host, BlobTable &blobs, HostBlobTable &host_blobs, WireRecorder &recorder,
                 ContextPlane &contexts);
    BlobCommands(const BlobCommands &) = delete;
    BlobCommands &operator=(const BlobCommands &) = delete;

    bool answer(ControlRequest &request, uint32_t type);

private:
    static constexpr uint64_t no_context_object = 0u;
    static constexpr uint64_t request_mask = 0xFFFFFFFFu;
    static constexpr size_t   attach_header_size = 32u;

    uint32_t create_answer(Status status) const;
    uint32_t create(ControlRequest &request);
    uint32_t create_guest(ControlRequest &request, uint32_t id, uint32_t entries, uint64_t size);
    uint32_t create_host(const CreateBlobRequest &create);
    uint32_t create_adopted(const CreateBlobRequest &create);
    bool     attach(ControlRequest &request, GpuResponse &response);
    void     map(ControlRequest &request, GpuResponse &response);
    uint32_t unmap(ControlRequest &request);
    bool     unref(ControlRequest &request, GpuResponse &response);
    uint32_t named_resource(ControlRequest &request);

    Host         &host_;
    BlobTable    &blobs_;
    HostBlobTable &host_blobs_;
    WireRecorder &recorder_;
    ContextPlane &contexts_;
};

}

#endif
