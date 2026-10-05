/* One consumer connected to a device's export socket (docs/external-export.md):
 * the descriptor the glue accepted, a thread of its own that reads the
 * consumer's requests and answers each in turn, and the surfaces it exported
 * and still holds - which are the ones it is told of when the guest lets go of
 * them or the device is reset.
 *
 * The thread holds the connection's lock from a request to its answer, and a
 * revocation takes it too, so an export the guest's release overtakes is
 * answered before the revocation that follows it, never after. The thread
 * never closes the descriptor: the main loop does once it has joined it, so
 * the number is not handed to another connection while the thread may still
 * use it. */
#ifndef AV_EXPORT_CONNECTION_HPP
#define AV_EXPORT_CONNECTION_HPP

#include "core/support/status.hpp"
#include "proto/export/export_operation.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExportAnswer;
class ExportRequest;
class ExportedGpu;
class Host;
struct ExportConnectionProbe;

class ExportConnection {
public:
    /* Surfaces one consumer holds at once: a mirror's ring of slots for each
     * window it shows, with room to spare. */
    static constexpr uint32_t max_surfaces = 64u;

    ExportConnection(Host &host, const ExportedGpu &gpu);
    ~ExportConnection();
    ExportConnection(const ExportConnection &) = delete;
    ExportConnection &operator=(const ExportConnection &) = delete;

    /* Main loop. `descriptor` served on a thread of its own, the connection's
     * from now on: invalid_state while it serves another; no_memory when the
     * host has no mutex or no thread. Refused, it stays the caller's. */
    [[nodiscard]] Status open(int descriptor);
    /* The thread's body, entered through AvExportConnectionServe: until the
     * consumer goes, or an answer cannot be sent. */
    void                 serve();
    /* Main loop, through the service. Its consumer is told `surface` is gone,
     * if it holds it, and holds it no more; 0, at a reset, is every surface,
     * and the consumer is told once whatever it held. Nothing once the
     * consumer has gone. */
    void                 revoke(uint64_t surface, ExportRevocation reason);
    /* Main loop. Serving, and whether its consumer has gone. */
    bool                 serving() const;
    bool                 ended() const;
    /* Main loop. The thread stopped and joined, the descriptor closed and
     * every surface let go: free to serve another. */
    void                 close();
    uint32_t             held() const;

    bool invariants_hold() const;

private:
    /* Under the lock: one request's bytes answered. Whether the answer was
     * sent. */
    bool         answer(const uint8_t *bytes, size_t length);
    ExportAnswer respond(const ExportRequest &request, int &descriptor);
    bool         send(const ExportAnswer &answer, int descriptor);
    bool         holds(uint64_t surface) const;
    void         drop(uint64_t surface);

    Host              &host_;
    const ExportedGpu &gpu_;
    void              *lock_;
    void              *thread_;
    int                descriptor_;
    /* Written by the thread as it leaves, under the lock. */
    bool               ended_;
    uint64_t           surfaces_[max_surfaces];
    uint32_t           held_;

    friend struct ExportConnectionProbe;
};

}  // namespace av

#endif  // AV_EXPORT_CONNECTION_HPP
