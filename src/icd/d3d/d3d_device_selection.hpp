/* One Direct3D device's commands on its process's stream: before each is lent
 * a writer, the stream is told which device speaks, since the host runs each
 * command on its own device's pipeline state (D3dCommandStream::speak_as).
 * Everything else is the stream's. A device's channel takes the process's
 * lock around this (LockedTransport), so the selection and the command it is
 * for go together. */
#ifndef AV_D3D_DEVICE_SELECTION_HPP
#define AV_D3D_DEVICE_SELECTION_HPP

#include "proto/client/client_transport.hpp"

#include <stdint.h>

namespace av::d3d {

class D3dCommandStream;

class D3dDeviceSelection final : public ClientTransport {
public:
    /* `device` is the id the host knows the device by; 0 selects nothing. */
    D3dDeviceSelection(D3dCommandStream &stream, uint32_t device);
    D3dDeviceSelection(const D3dDeviceSelection &) = delete;
    D3dDeviceSelection &operator=(const D3dDeviceSelection &) = delete;

    /* A selection that could not be sent fails the stream, and the writer it
     * lends then has no room, so the command fails where it is sent. */
    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

private:
    D3dCommandStream &stream_;
    uint32_t          device_;
};

}  // namespace av::d3d

#endif  // AV_D3D_DEVICE_SELECTION_HPP
