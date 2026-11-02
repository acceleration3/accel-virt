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
    static constexpr int inherited_socket = 3;

    RendererProcess();
    RendererProcess(const RendererProcess &) = delete;
    RendererProcess &operator=(const RendererProcess &) = delete;

    int run();

private:
    bool serve_message();

    GuestMemoryTable     memory_;
    DoorbellOffers       doorbells_;
    PosixRendererHost    host_;
    RendererService      service_;
    PosixRendererChannel channel_;
    uint8_t              datagram_[RendererMessageHeader::encoded_size + RendererMessageHeader::max_body_length];
    uint8_t              answer_[RendererService::answer_capacity];
};

}

#endif
