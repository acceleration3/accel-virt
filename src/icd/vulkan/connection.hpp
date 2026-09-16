/* One VkInstance's way to the host: the rvkgpu adapter, a device and paging
 * queue on it, and one Vulkan rvk context whose transport every object under the
 * instance sends through (research.md §3). The host's handles are per context,
 * so nothing made through an instance can cross another's connection.
 *
 * Opening is all or nothing, and any reason it cannot open - no rvkgpu adapter,
 * no LUID for it, a device the driver refuses, a context whose capset offers no
 * Vulkan - is the same answer to the loader: this driver does not serve here.
 * The transport stages its largest command, 256 KiB (research.md §4), in memory
 * taken when it opens: a connection lives in an ObjectPool chunk beside 63
 * others, so that room cannot be a member, and the transport is built over it
 * then. A transport the host has failed stays faulted, refusing every send
 * without a D3DKMT call, until the connection is closed. */
#ifndef AV_ICD_CONNECTION_HPP
#define AV_ICD_CONNECTION_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/vulkan/locked_transport.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class HostVisibleChunk;
class Kmt;
}  // namespace av

namespace av::icd {

class ThreadLock;
struct ConnectionProbe;

class Connection {
public:
    static constexpr uint8_t  ring_order = 20u;
    static constexpr uint32_t reply_bytes = 64u * 1024u;
    static constexpr size_t   command_bytes = 256u * 1024u;

    Connection(Kmt &kmt, BlockSource &memory, ThreadLock &lock);
    ~Connection();
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    /* invalid_state once open; no_memory when the staging cannot be had;
     * not_supported for every other reason it cannot open. Nothing is left open
     * unless ok. */
    [[nodiscard]] Status open();
    /* Closes the context, the device and the adapter; nothing when not open. */
    void                 close();
    bool                 opened() const;
    bool                 faulted() const;
    /* The rvkgpu adapter's handle, 0 until open. */
    uint32_t             adapter() const;
    /* The adapter's LUID, 0 until open. */
    uint64_t             luid() const;
    /* What the generated wrappers send through, only while open: the root
     * closes a connection only as it destroys it. */
    ClientTransport     &transport();
    /* `length` bytes of commands a CommandBatch framed, published as one
     * submission under the lock, only while open; failed for a batch the
     * transport refuses whole or a faulted transport. None of them asks for an
     * answer, so none is waited for. */
    [[nodiscard]] TransportResult send_batch(const uint8_t *framed, size_t length);

    /* A host-visible chunk of `bytes` built in `chunk`: allocated on this
     * connection's device, resident, locked and bound to its context, under
     * the lock. invalid_state while not open, faulted, or with `chunk` already
     * holding one, and nothing asked of the Kmt; otherwise the first refusal,
     * with whatever was made given back and `chunk` empty. */
    [[nodiscard]] Status open_chunk(ObjectStorage<HostVisibleChunk> &chunk, uint64_t bytes);
    /* Unbinds, unlocks and destroys the chunk `chunk` holds, under the lock,
     * whether or not the connection has faulted: the first refusal, and
     * `chunk` empty either way. */
    [[nodiscard]] Status close_chunk(ObjectStorage<HostVisibleChunk> &chunk);

    bool invariants_hold() const;

private:
    Kmt                           &kmt_;
    BlockSource                   &memory_;
    ThreadLock                    &lock_;
    AdapterSearch                  search_;
    GuestConnection                connection_;
    uint8_t                       *staging_;
    ObjectStorage<KmtTransport>    transport_;
    ObjectStorage<LockedTransport> locked_;
    uint64_t                       luid_;

    friend struct ConnectionProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_CONNECTION_HPP
