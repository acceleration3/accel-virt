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
    [[nodiscard]] bool make_room_for_blobs(const uint64_t *sizes, uint32_t count) override;
    ClientBlobLease    open_region(uint64_t bytes) override;
    void               close_region(const ClientBlobLease &region) override;
    const TransferRoom &room() const override;
    void note(ChannelCounter counter, uint64_t amount) override;

private:
    D3dCommandStream &stream_;
    uint32_t          device_;
};

}

#endif
