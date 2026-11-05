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
}

namespace av::d3d {

class D3dCommandStream;

class D3dDeviceChannel final : public ClientTransport {
public:
    D3dDeviceChannel(D3dCommandStream &stream, ThreadLock &lock, uint32_t device);
    D3dDeviceChannel(const D3dDeviceChannel &) = delete;
    D3dDeviceChannel &operator=(const D3dDeviceChannel &) = delete;

    uint32_t device() const;
    [[nodiscard]] bool   open(uint32_t feature_level, uint32_t &hresult, uint32_t &level);
    [[nodiscard]] bool   open9(uint32_t &hresult);
    [[nodiscard]] Status flush();
    [[nodiscard]] Status application_flush();
    [[nodiscard]] Status run_held();
    [[nodiscard]] Status take_recorded(uint8_t *out, size_t capacity, size_t &length);
    size_t               recorded_bytes();
    [[nodiscard]] Status execute(const uint8_t *commands, size_t length);
    [[nodiscard]] bool   close();
    [[nodiscard]] bool   close9();

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

    bool invariants_hold() const;

private:
    D3dCommandStream &stream_;
    ThreadLock       &lock_;
    uint32_t          device_;
    uint64_t          cleared_at_;
    D3dDeviceSelection selection_;
    icd::LockedTransport locked_;
};

}

#endif
