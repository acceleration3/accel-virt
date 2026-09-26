/* A renderer that answers from a script, for tests of QEMU's end of the
 * socket: it keeps the kind, sequence, descriptor count and body of every
 * datagram it is sent, closing the duplicates it is handed, and answers each
 * hello with a Ready and each request with a reply carrying the outcome the
 * test set - unless it was told to fall silent. Datagrams a test queues are
 * answered before those, so a stale reply, a wrong kind, garbage or a
 * datagram carrying descriptors can stand between a request and its answer. */
#ifndef AV_SCRIPTED_RENDERER_PEER_HPP
#define AV_SCRIPTED_RENDERER_PEER_HPP

#include "core/support/status.hpp"
#include "harness/host/renderer_peer.hpp"
#include "proto/renderer/ready_message.hpp"
#include "proto/renderer/renderer_message_header.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class MockHost;

class ScriptedRendererPeer final : public RendererPeer {
public:
    static constexpr std::uint32_t max_datagrams = 128u;
    static constexpr std::uint32_t max_queued = 16u;
    static constexpr std::size_t   max_body = 256u;
    static constexpr std::uint32_t max_attached = 4u;

    explicit ScriptedRendererPeer(MockHost &host);
    ~ScriptedRendererPeer();

    void deliver(const std::uint8_t *bytes, std::size_t length, const int *descriptors,
                 std::uint32_t count) override;
    bool answer(std::uint8_t *buffer, std::size_t capacity, std::size_t &length, int *descriptors,
                std::uint32_t capacity_descriptors, std::uint32_t &count) override;

    /* What each request is answered with from here on. */
    void answer_requests_with(Status outcome);
    /* What each hello is answered with from here on. */
    void ready_with(const ReadyMessage &ready);
    /* Nothing is answered from here on, as a renderer that hung. */
    void fall_silent();
    /* The next datagram delivered makes the renderer die, as MockHost's
     * renderer_gone says, before it answers. */
    void die_on_next_delivery();
    /* A datagram answered before any scripted answer, carrying `descriptors`
     * fresh descriptors of this process. */
    void queue(const std::uint8_t *bytes, std::size_t length, std::uint32_t descriptors);
    /* A reply framed as a renderer frames one: header and body both naming
     * `sequence`. */
    void queue_reply(std::uint64_t sequence, Status outcome, std::uint32_t descriptors);
    /* A reply whose header names `header_sequence` and whose body names
     * `body_sequence`. */
    void queue_reply(std::uint64_t header_sequence, std::uint64_t body_sequence, Status outcome);

    std::uint32_t               delivered() const;
    RendererMessageHeader::Kind kind_at(std::uint32_t index) const;
    std::uint64_t               sequence_at(std::uint32_t index) const;
    std::uint32_t               descriptors_at(std::uint32_t index) const;
    /* Descriptors delivered that were open when they arrived. */
    std::uint32_t               open_descriptors_at(std::uint32_t index) const;
    const std::uint8_t         *body_at(std::uint32_t index) const;
    std::uint32_t               body_length_at(std::uint32_t index) const;
    std::uint32_t               queued() const;

private:
    struct Datagram {
        RendererMessageHeader::Kind kind;
        std::uint64_t               sequence;
        std::uint32_t               descriptors;
        std::uint32_t               open;
        std::uint8_t                body[max_body];
        std::uint32_t               body_length;
    };
    struct Answer {
        std::uint8_t  bytes[RendererMessageHeader::encoded_size + max_body];
        std::size_t   length;
        std::uint32_t descriptors;
    };

    void push(const std::uint8_t *bytes, std::size_t length, std::uint32_t descriptors);
    void answer_request(const RendererMessageHeader &header);

    MockHost     &host_;
    Datagram      datagrams_[max_datagrams];
    std::uint32_t delivered_;
    Answer        answers_[max_queued];
    std::uint32_t first_;
    std::uint32_t queued_;
    Status        outcome_;
    ReadyMessage  ready_;
    bool          silent_;
    bool          dying_;
};

}  // namespace av

#endif  // AV_SCRIPTED_RENDERER_PEER_HPP
