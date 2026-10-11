/* SHARE_OBJECT, OPEN_SHARED_OBJECT or RELEASE_SHARED_OBJECT, the transport
 * commands that give one host object an identity the whole device agrees on
 * (docs/protocol.md, Transport commands; feature 053).
 *
 * Framed exactly as BlobImportCommand frames its two: a body no producer
 * writes is command framing, which faults the context as a NOP with a body
 * does; every other refusal is a status in the answer, and the context runs
 * on. Which of the three may carry a wire handle is this class's rule, because
 * the body alone cannot know which command it is inside. */
#ifndef AV_SHARED_OBJECT_COMMAND_HPP
#define AV_SHARED_OBJECT_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextExecutor;
class SharedObjectRequest;

class SharedObjectCommand {
public:
    /* `body` is the header's size less the header's bytes, as the stream
     * framed it. */
    SharedObjectCommand(ContextExecutor &executor, const CommandHeader &header, const uint8_t *body);

    /* Worker. malformed for a body of any length but the request's;
     * not_found for any other transport id; ok otherwise, with `produced` the
     * answer's length and the answer in `reply` when `capacity` holds it. */
    [[nodiscard]] Status run(uint8_t *reply, size_t capacity, size_t &produced);

private:
    /* The status a refusal of this command is, given what the executor
     * answered. */
    uint32_t             refusal_of(Status answered) const;
    [[nodiscard]] Status answer(uint32_t status, uint64_t wire_handle, uint8_t *reply, size_t capacity,
                                size_t &produced);
    /* Whether this command may carry a wire handle: only a share names an
     * object the guest already holds. */
    bool                 handle_allowed() const;
    size_t               body_size() const;

    ContextExecutor &executor_;
    CommandHeader    header_;
    const uint8_t   *body_;
};

}  // namespace av

#endif  // AV_SHARED_OBJECT_COMMAND_HPP
