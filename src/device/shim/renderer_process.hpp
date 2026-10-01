/* av-renderer: the process QEMU starts to run a device's contexts
 * (docs/renderer.md). It holds the guest memory QEMU mapped into it, the
 * doorbells QEMU offered, the host the device core runs over, and the service
 * that carries out QEMU's messages; this is its control loop. */
#ifndef AV_RENDERER_PROCESS_HPP
#define AV_RENDERER_PROCESS_HPP

#include "device/renderer/doorbell_offers.hpp"
#include "device/renderer/guest_memory_table.hpp"
#include "device/renderer/renderer_service.hpp"
#include "device/shim/posix_renderer_channel.hpp"
#include "device/shim/posix_renderer_host.hpp"
#include "proto/renderer/renderer_message_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RendererProcess {
public:
    /* The socket QEMU passed as this descriptor. */
    static constexpr int inherited_socket = 3;

    RendererProcess();
    RendererProcess(const RendererProcess &) = delete;
    RendererProcess &operator=(const RendererProcess &) = delete;

    /* Until QEMU goes: 0 when it closed the socket, 1 when this process could
     * not start. */
    int run();

private:
    /* One message in, its answer out: false once QEMU has gone. */
    bool serve_message();

    GuestMemoryTable     memory_;
    DoorbellOffers       doorbells_;
    PosixRendererHost    host_;
    RendererService      service_;
    PosixRendererChannel channel_;
    uint8_t              datagram_[RendererMessageHeader::encoded_size + RendererMessageHeader::max_body_length];
    uint8_t              answer_[RendererService::answer_capacity];
};

}  // namespace av

#endif  // AV_RENDERER_PROCESS_HPP
