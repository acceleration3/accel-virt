/* One VkInstance's way to the host: the rvkgpu adapter, a device and paging
 * queue on it, and one Vulkan rvk context whose transport every object under the
 * instance sends through (research.md §3). The host's handles are per context,
 * so nothing made through an instance can cross another's connection.
 *
 * Opening is all or nothing, and any reason it cannot open - no rvkgpu adapter,
 * no LUID for it, a device the driver refuses, a context whose capset offers no
 * Vulkan - is the same answer to the loader: this driver does not serve here.
 * A transport the host has failed stays faulted, refusing every send without a
 * D3DKMT call, until the connection is closed. */
#ifndef AV_ICD_CONNECTION_HPP
#define AV_ICD_CONNECTION_HPP

#include "core/support/status.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/vulkan/locked_transport.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
class Kmt;
}  // namespace av

namespace av::icd {

class AdapterLuids;
class ThreadLock;
struct ConnectionProbe;

class Connection {
public:
    /* The ring the transport is set up with. Integration point: T023 gives
     * KmtTransport a ring order, command limit and reply size at construction,
     * and this becomes order 20 with 256 KiB commands and 64 KiB replies. */
    static constexpr uint8_t ring_order = 12u;

    Connection(Kmt &kmt, AdapterLuids &luids, ThreadLock &lock);
    ~Connection();
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    /* invalid_state once open; not_supported for every reason it cannot open,
     * with nothing left open. */
    [[nodiscard]] Status open();
    /* Closes the context, the device and the adapter; nothing when not open. */
    void                 close();
    bool                 opened() const;
    bool                 faulted() const;
    /* The adapter's LUID, 0 until open. */
    uint64_t             luid() const;
    /* What the generated wrappers send through; a send on a connection that is
     * not open fails. */
    ClientTransport     &transport();

    bool invariants_hold() const;

private:
    AdapterLuids   &luids_;
    AdapterSearch   search_;
    GuestConnection connection_;
    KmtTransport    transport_;
    LockedTransport locked_;
    uint64_t        luid_;

    friend struct ConnectionProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_CONNECTION_HPP
