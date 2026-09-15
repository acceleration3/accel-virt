#include "icd/vulkan/connection.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "icd/vulkan/thread_lock.hpp"
#include "proto/context/sub_api.hpp"

namespace av::icd {

Connection::Connection(Kmt &kmt, BlockSource &memory, ThreadLock &lock)
    : kmt_(kmt), memory_(memory), lock_(lock), search_(kmt), connection_(kmt), staging_(nullptr), transport_(),
      locked_(), luid_(0u) {}

Connection::~Connection() { close(); }

Status Connection::open()
{
    if (opened()) return Status::invalid_state;
    ScopeExit undo([&] { close(); });
    const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                             AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
    if (search_.find(rvkgpu) != Status::ok) return Status::not_supported;
    if (kmt_.adapter_luid(search_.adapter(), luid_) != Status::ok) return Status::not_supported;
    if (connection_.open(search_.adapter()) != Status::ok) return Status::not_supported;
    staging_ = static_cast<uint8_t *>(memory_.allocate(command_bytes, Pool::paged));
    if (staging_ == nullptr) return Status::no_memory;
    /* Neither storage holds an object here: close() empties both on every way
     * out, and an open connection was refused above. */
    static_cast<void>(transport_.construct(connection_, SubApi::vulkan, ring_order, reply_bytes, staging_,
                                           command_bytes));
    if (transport_.object().initialize() != Status::ok) return Status::not_supported;
    static_cast<void>(locked_.construct(transport_.object(), lock_));
    undo.dismiss();
    return Status::ok;
}

void Connection::close()
{
    locked_.destroy();
    transport_.destroy();
    if (staging_ != nullptr) memory_.release(staging_);
    staging_ = nullptr;
    connection_.close();
    /* Nothing is left to report a failed close to: the instance is gone. */
    static_cast<void>(search_.close());
    luid_ = 0u;
}

bool             Connection::opened() const { return locked_.constructed(); }
bool             Connection::faulted() const { return opened() && transport_.object().faulted(); }
uint64_t         Connection::luid() const { return luid_; }
ClientTransport &Connection::transport() { return locked_.object(); }

TransportResult Connection::send_batch(const uint8_t *framed, size_t length)
{
    lock_.acquire();
    ScopeExit  release([&] { lock_.release(); });
    WireReader unanswered(nullptr, 0u);
    return transport_.object().send_batch(framed, length, unanswered);
}

bool Connection::invariants_hold() const
{
    if (!connection_.invariants_hold()) return false;
    if (!opened()) return !transport_.constructed() && staging_ == nullptr && !connection_.opened() && !search_.found();
    if (!transport_.constructed() || !transport_.object().invariants_hold()) return false;
    return staging_ != nullptr && connection_.opened() && search_.found() && connection_.adapter() == search_.adapter();
}

}  // namespace av::icd
