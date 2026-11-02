#ifndef AV_EXPORT_SERVICE_HPP
#define AV_EXPORT_SERVICE_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/export/export_connection.hpp"
#include "device/export/exported_gpu.hpp"
#include "proto/export/export_operation.hpp"

#include <stdint.h>

namespace av {

class ContextPlane;
class HeadPlanes;
class DisplayClock;
class ExportGreeting;
class Host;
struct ExportServiceProbe;

class ExportService {
public:
    static constexpr uint32_t max_connections = 8u;

    ExportService(Host &host, DisplayClock &clock);
    ~ExportService();
    ExportService(const ExportService &) = delete;
    ExportService &operator=(const ExportService &) = delete;

    void                 attach(ContextPlane &contexts, const uint8_t *gpu_uuid, const ExportGreeting &greeting,
                         HeadPlanes &heads);
    [[nodiscard]] Status accept(int connection, bool same_user);
    void                 revoke(uint64_t surface, ExportRevocation reason);
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    Host                          &host_;
    ExportedGpu                    gpu_;
    ObjectStorage<ExportConnection> connections_[max_connections];

    friend struct ExportServiceProbe;
};

}

#endif
