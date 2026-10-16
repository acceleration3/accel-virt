/* One VkInstance's way to the host: the avgpu adapter, a device and paging
 * queue on it, and one Vulkan av context whose transport every object under the
 * instance sends through (research.md §3). The host's handles are per context,
 * so nothing made through an instance can cross another's connection.
 *
 * Opening is all or nothing, and any reason it cannot open - no avgpu adapter,
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
#include "guest/kmt/adapter_search.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/adapter_facts.hpp"
#include "icd/common/host_channel.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
class ClientTransport;
class Kmt;
class StateWriter;
class ThreadLock;
}  // namespace av

namespace av::icd {

struct ConnectionProbe;

class Connection {
public:
    static constexpr uint8_t  ring_order = 20u;
    static constexpr uint32_t reply_bytes = 64u * 1024u;
    static constexpr size_t   command_bytes = 256u * 1024u;

    /* `log` takes the lines the channel writes, and `lock` is its send lock. */
    Connection(Kmt &kmt, BlockSource &memory, ThreadLock &lock, LogSink &log);
    ~Connection();
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    /* On the avgpu adapter whose LUID is `luid`; 0 takes the one the primary
     * display is on (AdapterSearch::find_avgpu). invalid_state once open;
     * no_memory when the staging cannot be had; not_supported for every other
     * reason it cannot open. Nothing is left open unless ok. */
    [[nodiscard]] Status open(uint64_t luid);
    /* Closes the context, the device and the adapter; nothing when not open. */
    void                 close();
    bool                 opened() const;
    bool                 faulted() const;
    /* The avgpu adapter's handle, 0 until open. */
    uint32_t             adapter() const;
    /* The D3DKMT device the connection made; 0 unless open. */
    uint32_t             device() const;
    /* The adapter's LUID, 0 until open. */
    uint64_t             luid() const;
    /* What Windows calls the adapter - the name Device Manager, DXGI and the
     * OpenCL ICD show - read as it opens; empty until open, or where Windows
     * gives none. */
    const char          *adapter_name() const;
    /* Remembered from the one time the host was asked for its device
     * extensions: whether it has VK_EXT_external_memory_dma_buf, which decides
     * what a shared allocation is exported as. A host never asked is taken not
     * to have it, which is the answer that shares by an opaque fd - what every
     * host did before feature 053. */
    void                 note_host_dma_buf();
    bool                 host_has_dma_buf() const;
    /* What the generated wrappers send through, only while open: the root
     * closes a connection only as it destroys it. */
    ClientTransport     &transport();
    /* Batches sent on this connection so far, every kind. */
    uint64_t             submissions() const;
    /* `length` bytes of commands a CommandBatch framed, published as one
     * submission under the lock, only while open; failed for a batch the
     * transport refuses whole or a faulted transport. None of them asks for an
     * answer, so none is waited for. */
    [[nodiscard]] TransportResult send_batch(const uint8_t *framed, size_t length);
    /* A recorded command too long for any batch, sent alone under the lock
     * (HostChannel::send_command), only while open. */
    [[nodiscard]] TransportResult send_command(uint32_t command_id, WireWriter &writer);

    /* The channel the connection's host memory is opened on and kept by,
     * for each device of its instance (HostChannel::open_memory). */
    HostChannel         &channel();

    /* Its fields on the line `writer` has open: whether open and faulted, and
     * the adapter, LUID, device and context it sends through. */
    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    Kmt                           &kmt_;
    AdapterSearch                  search_;
    GuestConnection                connection_;
    HostChannel                    channel_;
    /* Open from a whole open to the close: the channel opened alone is a
     * connection that was not. */
    bool                           opened_;
    /* What Windows says of the adapter: its LUID, and the name Vulkan shows. */
    AdapterFacts                   adapter_;
    bool                           host_dma_buf_;

    friend struct ConnectionProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_CONNECTION_HPP
