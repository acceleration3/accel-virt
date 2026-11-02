#include "device/export/export_service.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

ExportService::ExportService(Host &host, DisplayClock &clock) : host_(host), gpu_(host), connections_()
{
    AV_SCOPE("ExportService::ExportService");
    for (ObjectStorage<ExportConnection> &storage : connections_)
        static_cast<void>(storage.construct(host, gpu_, clock));
}

ExportService::~ExportService()
{
    AV_SCOPE("ExportService::~ExportService");
    for (ObjectStorage<ExportConnection> &storage : connections_) storage.destroy();
}

void ExportService::attach(ContextPlane &contexts, const uint8_t *gpu_uuid, const ExportGreeting &greeting,
                           HeadPlanes &heads)
{
    AV_SCOPE("ExportService::attach");
    gpu_.attach(contexts, gpu_uuid, greeting, heads);
}

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

}
