#ifndef AV_D3D_DEVICE_SELECTION_HPP
#define AV_D3D_DEVICE_SELECTION_HPP

#include "proto/client/client_transport.hpp"

#include <stdint.h>

namespace av::d3d {

class D3dCommandStream;

class D3dDeviceSelection final : public ClientTransport {
public:
    D3dDeviceSelection(D3dCommandStream &stream, uint32_t device);
    D3dDeviceSelection(const D3dDeviceSelection &) = delete;
    D3dDeviceSelection &operator=(const D3dDeviceSelection &) = delete;

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;
    [[nodiscard]] bool make_room_for_blobs(size_t bytes) override;
    const TransferRoom &room() const override;

private:
    D3dCommandStream &stream_;
    uint32_t          device_;
};

}

#endif
