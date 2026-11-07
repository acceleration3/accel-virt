#ifndef AV_D3D12_SESSION_LOOPBACK_HPP
#define AV_D3D12_SESSION_LOOPBACK_HPP

#include "core/log/driver_log.hpp"
#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d12_session.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/d3d/watched_blocks.hpp"
#include "executor/eventfd_wake.hpp"
#include "executor/import_table.hpp"
#include "executor/d3d/d3d12_object_holds.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/wake_signal.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/transport/roomed_transport.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "proto/wire/wire_arena.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class FakeDevice;

class D3d12SessionLoopback final : public RoomedTransport {
public:
    static constexpr std::size_t command_bytes = 1u << 16u;
    static constexpr std::size_t reply_bytes = 4096u;
    static constexpr std::size_t arena_bytes = 1u << 16u;

    D3d12SessionLoopback();
    ~D3d12SessionLoopback();
    D3d12SessionLoopback(const D3d12SessionLoopback &) = delete;
    D3d12SessionLoopback &operator=(const D3d12SessionLoopback &) = delete;

    D3d12Session   &session();
    D3dObjectTable &objects();
    ImportTable    &imports();
    uint32_t        refused() const;
    void            place(FakeDevice &device);
    D3d12QueryHeaps &queries();
    D3d12ObjectHolds &holds();
    void             hurry(uint64_t nanoseconds);

    WireWriter      request_writer() override;
    TransportResult send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void            end_call() override;
    WireArena      &decode_arena() override;
    ClientBlobArena &blobs() override;

private:
    bool run(uint32_t command_id, WireWriter &writer, bool awaited, std::size_t &answered);

    HostHeap                    heap_;
    WatchedBlocks               blocks_;
    D3dObjectTable              objects_;
    WakeSignal                  tenant_;
    EventfdWake                 fence_wake_;
    RecordingLog                sink_;
    DriverLog                   lines_;
    ImportTable                 imports_;
    D3d12HostDevice             host_;
    GuestBytes                  guest_;
    alignas(64) std::uint8_t    guest_memory_[8192];
    ObjectStorage<D3d12Session> storage_;
    std::uint8_t                command_[command_bytes];
    std::uint8_t                reply_[reply_bytes];
    alignas(16) std::uint8_t    arena_block_[arena_bytes];
    WireArena                   arena_;
    WireArena                   decode_arena_;
    ClientBlobArena             blobs_;
    uint32_t                    refused_;
    FakeDevice                 *placed_;
};

}

#endif
