#ifndef AV_ICD_CONNECTION_HPP
#define AV_ICD_CONNECTION_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/adapter_facts.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/loss_latch.hpp"
#include "icd/common/opened_objects.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class KeyedLog;
class Kmt;
class StateWriter;
class ThreadLock;
}

namespace av::icd {

class SharedHandles;
struct ConnectionProbe;

class Connection {
public:
    static constexpr uint8_t  ring_order = 20u;
    static constexpr uint32_t reply_bytes = 64u * 1024u;
    static constexpr size_t   command_bytes = 256u * 1024u;

    Connection(Kmt &kmt, BlockSource &memory, ThreadLock &lock, KeyedLog &lines, SharedHandles &handles);
    ~Connection();
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    [[nodiscard]] Status open(uint64_t luid);
    void                 close();
    bool                 opened() const;
    bool                 lost() const;
    [[nodiscard]] bool   refuses(const char *entry);
    uint32_t             adapter() const;
    uint32_t             device() const;
    uint64_t             luid() const;
    const char          *adapter_name() const;
    [[nodiscard]] bool   wake_on(uint64_t handle);
    bool                 wakes() const;
    void                 note_host_dma_buf();
    bool                 host_has_dma_buf() const;
    ClientTransport     &transport();
    uint64_t             submissions() const;
    [[nodiscard]] TransportResult send_batch(const uint8_t *framed, size_t length);
    [[nodiscard]] TransportResult send_command(uint32_t command_id, WireWriter &writer);

    HostChannel         &channel();
    OpenedObjects       &opened_objects();

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    Kmt                           &kmt_;
    AdapterSearch                  search_;
    GuestConnection                connection_;
    HostChannel                    channel_;
    LossLatch                      loss_;
    bool                           opened_;
    AdapterFacts                   adapter_;
    bool                           host_dma_buf_;
    bool                           wakes_;
    OpenedObjects                  opened_objects_;

    friend struct ConnectionProbe;
};

}

#endif
