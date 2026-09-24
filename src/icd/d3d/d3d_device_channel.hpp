/* One Direct3D device's way onto its process's command stream.
 *
 * The runtime calls a device from whichever thread the application uses, and
 * each of a process's devices may be on a different one, all recording into
 * the one stream. So a command is recorded under the process's lock, and the
 * stream is told which device is speaking before it is lent a writer: the
 * host runs each command on its own device's pipeline state. The lock is held
 * from the writer being lent until an asynchronous command is recorded or a
 * synchronous call ends, as LockedTransport holds it, so another device's
 * round trip cannot replace a reply mid-decode. */
#ifndef AV_D3D_DEVICE_CHANNEL_HPP
#define AV_D3D_DEVICE_CHANNEL_HPP

#include "core/support/status.hpp"
#include "proto/client_transport.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::icd {
class ThreadLock;
}  // namespace av::icd

namespace av::d3d {

class D3dCommandStream;

class D3dDeviceChannel final : public ClientTransport {
public:
    /* `device` is the id the host knows the device by; 0 for a Direct3D 12
     * device, which the stream never selects and which is never opened or
     * closed through this. */
    D3dDeviceChannel(D3dCommandStream &stream, icd::ThreadLock &lock, uint32_t device);
    D3dDeviceChannel(const D3dDeviceChannel &) = delete;
    D3dDeviceChannel &operator=(const D3dDeviceChannel &) = delete;

    uint32_t device() const;
    /* Makes the device on the host (CreateDevice11), which selects it there.
     * Sent without a selection first: the host knows no device by this id
     * until the command runs, and refuses selecting one it does not know. */
    [[nodiscard]] bool   open(uint32_t feature_level, uint32_t &hresult, uint32_t &level);
    /* The stream's flush and make_room_for, under the lock. */
    [[nodiscard]] Status flush();
    [[nodiscard]] Status flush_hint();
    [[nodiscard]] bool   make_room_for(size_t bytes);
    /* The device is destroyed on the host, and the stream stops speaking for it. */
    [[nodiscard]] bool   close();

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

    /* Between calls a channel is idle: no call left its lock held. */
    bool invariants_hold() const;

private:
    enum class Phase : uint8_t { idle, writing, answering };

    void let_go();

    D3dCommandStream &stream_;
    icd::ThreadLock  &lock_;
    uint32_t          device_;
    Phase             phase_;
};

}  // namespace av::d3d

#endif  // AV_D3D_DEVICE_CHANNEL_HPP
