#include "device/export/export_service.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

ExportService::ExportService(Host &host) : host_(host), gpu_(host), connections_()
{
    AV_SCOPE("ExportService::ExportService");
    for (ObjectStorage<ExportConnection> &storage : connections_) static_cast<void>(storage.construct(host, gpu_));
}

/* Every connection's thread ends before the GPU it asks goes. */
ExportService::~ExportService()
{
    AV_SCOPE("ExportService::~ExportService");
    for (ObjectStorage<ExportConnection> &storage : connections_) storage.destroy();
}

void ExportService::attach(ContextPlane &contexts, const uint8_t *gpu_uuid, const ExportGreeting &greeting)
{
    AV_SCOPE("ExportService::attach");
    gpu_.attach(contexts, gpu_uuid, greeting);
}

/* The consumers that have gone are reaped first, so a full service still
 * takes a connection while one of its slots only waits to be closed. */
Status ExportService::accept(int connection, bool same_user)
{
    AV_SCOPE("ExportService::accept");
    host_.require_main_loop("ExportService::accept");
    if (!same_user) {
        host_.log("virtio-gpu-av: an export connection from another user's process was refused");
        return Status::invalid_parameter;
    }
    ExportConnection *free = nullptr;
    for (ObjectStorage<ExportConnection> &storage : connections_) {
        ExportConnection &candidate = storage.object();
        if (candidate.ended()) candidate.close();
        if (free == nullptr && !candidate.serving()) free = &candidate;
    }
    if (free == nullptr) {
        host_.log("virtio-gpu-av: an export connection was refused: every slot is serving one");
        return Status::no_space;
    }
    return free->open(connection);
}

void ExportService::revoke(uint64_t surface, ExportRevocation reason)
{
    AV_SCOPE("ExportService::revoke");
    host_.require_main_loop("ExportService::revoke");
    for (ObjectStorage<ExportConnection> &storage : connections_) storage.object().revoke(surface, reason);
}

uint32_t ExportService::live() const
{
    AV_SCOPE("ExportService::live");
    uint32_t serving = 0u;
    for (const ObjectStorage<ExportConnection> &storage : connections_) serving += storage.object().serving() ? 1u : 0u;
    return serving;
}

bool ExportService::invariants_hold() const
{
    AV_SCOPE("ExportService::invariants_hold");
    for (const ObjectStorage<ExportConnection> &storage : connections_)
        if (!storage.object().invariants_hold()) return false;
    return live() <= max_connections;
}

}  // namespace av
