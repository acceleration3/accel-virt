#ifndef AV_RENDERER_LINK_HPP
#define AV_RENDERER_LINK_HPP

#include "core/support/status.hpp"
#include "proto/renderer/renderer_message_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class Host;
class ReadyMessage;

class RendererLink {
public:
    static constexpr uint32_t request_timeout_ms = 2000u;
    static constexpr uint32_t hello_timeout_ms = 10000u;
    static constexpr uint32_t max_descriptors = 16u;

    explicit RendererLink(Host &host);
    RendererLink(const RendererLink &) = delete;
    RendererLink &operator=(const RendererLink &) = delete;

    [[nodiscard]] Status post(RendererMessageHeader::Kind kind, const uint8_t *body, uint32_t length,
                              const int *descriptors, uint32_t count);
    [[nodiscard]] Status request(RendererMessageHeader::Kind kind, const uint8_t *body, uint32_t length,
                                 const int *descriptors, uint32_t count, Status &outcome);
    [[nodiscard]] Status greet(const uint8_t *body, uint32_t length, ReadyMessage &ready);
    [[nodiscard]] Status exchange(RendererMessageHeader::Kind kind, const uint8_t *body, uint32_t length,
                                  RendererMessageHeader::Kind answered, const uint8_t *&answer, uint32_t &answer_length,
                                  int *descriptors, uint32_t capacity, uint32_t &count);

private:
    [[nodiscard]] Status await(RendererMessageHeader::Kind expected, uint64_t sequence, uint32_t timeout_ms,
                               const uint8_t *&body, uint32_t &length, int *kept, uint32_t capacity, uint32_t &count);
    [[nodiscard]] Status frame(RendererMessageHeader::Kind kind, const uint8_t *body, uint32_t length,
                               uint64_t &sequence);

    Host    &host_;
    uint64_t sequence_;
    size_t   framed_;
    uint8_t  datagram_[RendererMessageHeader::encoded_size + RendererMessageHeader::max_body_length];
};

}

#endif
