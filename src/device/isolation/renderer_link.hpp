/* QEMU's end of the socket to its renderer (docs/renderer.md): each message
 * framed with its kind and a sequence number, and the answer to one QEMU
 * waits for found by that number. A wait is bounded: a renderer that does not
 * answer in time is treated as one that died. Main loop only. */
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
    /* A context's create, bind or destroy. */
    static constexpr uint32_t request_timeout_ms = 2000u;
    /* Loading the executor and every host API it opens: an EGL device search
     * alone has taken three seconds. */
    static constexpr uint32_t hello_timeout_ms = 10000u;
    static constexpr uint32_t max_descriptors = 16u;

    explicit RendererLink(Host &host);
    RendererLink(const RendererLink &) = delete;
    RendererLink &operator=(const RendererLink &) = delete;

    /* A message nothing answers. not_found once the renderer is gone. */
    [[nodiscard]] Status post(RendererMessageHeader::Kind kind, const uint8_t *body, uint32_t length,
                              const int *descriptors, uint32_t count);
    /* A message and the reply the renderer owes it: its outcome in
     * `outcome`. not_found when the renderer is gone, invalid_state when it
     * did not answer in time, malformed when it answered with something no
     * renderer sends. */
    [[nodiscard]] Status request(RendererMessageHeader::Kind kind, const uint8_t *body, uint32_t length,
                                 const int *descriptors, uint32_t count, Status &outcome);
    /* The hello and the renderer's Ready. */
    [[nodiscard]] Status greet(const uint8_t *body, uint32_t length, ReadyMessage &ready);
    /* A message answered by one of kind `answered`: its body, valid until the
     * next call, and up to `capacity` descriptors that came with it, now the
     * caller's; any beyond that are closed. Fails as request does. */
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

}  // namespace av

#endif  // AV_RENDERER_LINK_HPP
