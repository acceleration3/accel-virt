/* A device's export socket (docs/external-export.md): the connections the
 * glue accepts on the socket the `export-socket` property names, each served
 * on a thread of its own, and the revocations the guest's releases and the
 * device's resets send them. Only a process of QEMU's own user is served, and
 * at most max_connections at once; a connection whose consumer has gone is
 * reaped when another arrives, and every one when the device goes. */
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
class DisplayClock;
class ExportGreeting;
class Host;
struct ExportServiceProbe;

class ExportService {
public:
    static constexpr uint32_t max_connections = 8u;

    /* Consumers' displays time `clock`. */
    ExportService(Host &host, DisplayClock &clock);
    ~ExportService();
    ExportService(const ExportService &) = delete;
    ExportService &operator=(const ExportService &) = delete;

    /* Main loop, at realize: where surfaces come from, and what hello
     * answers. */
    void                 attach(ContextPlane &contexts, const uint8_t *gpu_uuid, const ExportGreeting &greeting);
    /* Main loop. `connection`, which the glue accepted, served - and the
     * service's from now on - or refused, leaving it the glue's to close:
     * invalid_parameter for another user's process, no_space while
     * max_connections are served, no_memory when the host has no thread for
     * it. */
    [[nodiscard]] Status accept(int connection, bool same_user);
    /* Main loop. Every consumer holding `surface` - every consumer, for 0 -
     * told it is gone for `reason`. */
    void                 revoke(uint64_t surface, ExportRevocation reason);
    /* Connections being served, their consumers gone or not. */
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    Host                          &host_;
    ExportedGpu                    gpu_;
    ObjectStorage<ExportConnection> connections_[max_connections];

    friend struct ExportServiceProbe;
};

}  // namespace av

#endif  // AV_EXPORT_SERVICE_HPP
