/* One Direct3D device's way onto its process's command stream.
 *
 * The runtime calls a device from whichever thread the application uses, and
 * each of a process's devices may be on a different one, all recording into
 * the one stream. So a command is recorded under the process's lock, and the
 * stream is told which device is speaking before it is lent a writer
 * (D3dDeviceSelection): the host runs each command on its own device's
 * pipeline state. The lock is held from the writer being lent until an
 * asynchronous command is recorded or a synchronous call ends
 * (LockedTransport), so another device's round trip cannot replace a reply
 * mid-decode. */
#ifndef AV_D3D_DEVICE_CHANNEL_HPP
#define AV_D3D_DEVICE_CHANNEL_HPP

#include "core/support/status.hpp"
#include "icd/common/locked_transport.hpp"
#include "icd/d3d/d3d_device_selection.hpp"
#include "proto/client/client_transport.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class ThreadLock;
}  // namespace av

namespace av::d3d {

class D3dCommandStream;

class D3dDeviceChannel final : public ClientTransport {
public:
    /* `device` is the id the host knows the device by; 0 for a Direct3D 12
     * device, which the stream never selects and which is never opened or
     * closed through this. */
    D3dDeviceChannel(D3dCommandStream &stream, ThreadLock &lock, uint32_t device);
    D3dDeviceChannel(const D3dDeviceChannel &) = delete;
    D3dDeviceChannel &operator=(const D3dDeviceChannel &) = delete;

    uint32_t device() const;
    /* Makes the device on the host (CreateDevice11), which selects it there.
     * Sent without a selection first: the host knows no device by this id
     * until the command runs, and refuses selecting one it does not know. */
    [[nodiscard]] bool   open(uint32_t feature_level, uint32_t &hresult, uint32_t &level);
    /* The same for a Direct3D 9 device (CreateDevice9), on a stream told to
     * speak Direct3D 9's selection (D3dCommandStream::speak_with). */
    [[nodiscard]] bool   open9(uint32_t &hresult);
    /* The stream's flush, under the lock. */
    [[nodiscard]] Status flush();
    [[nodiscard]] Status application_flush();
    /* The publish barrier, under the lock (KmtTransport::run_held): before
     * another process opens what this device just made. */
    [[nodiscard]] Status run_held();
    /* A deferred context's recording, taken out of its stream. */
    [[nodiscard]] Status take_recorded(uint8_t *out, size_t capacity, size_t &length);
    size_t               recorded_bytes();
    /* A command list run on this device: from and back to the default state,
     * as ExecuteCommandList leaves an immediate context. */
    [[nodiscard]] Status execute(const uint8_t *commands, size_t length);
    /* The device is destroyed on the host, and the stream stops speaking for it. */
    [[nodiscard]] bool   close();
    /* The same for a Direct3D 9 device (Destroy9). */
    [[nodiscard]] bool   close9();

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;
    /* The stream's, under the lock, for data past what crosses inline either
     * way (TransferRoom::inline_both); that needs none. */
    [[nodiscard]] bool make_room_for_blobs(size_t bytes) override;
    const TransferRoom &room() const override;

    /* The lock is held exactly while a command is written or a reply read,
     * and the stream is sound. */
    bool invariants_hold() const;

private:
    D3dCommandStream &stream_;
    ThreadLock       &lock_;
    uint32_t          device_;
    /* The stream's recorded_total() just after a command list's closing
     * ClearState11: a list executed right after needs no opening one. */
    uint64_t          cleared_at_;
    D3dDeviceSelection selection_;
    icd::LockedTransport locked_;
};

}  // namespace av::d3d

#endif  // AV_D3D_DEVICE_CHANNEL_HPP
