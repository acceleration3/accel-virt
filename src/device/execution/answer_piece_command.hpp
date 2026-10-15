/* ANSWER_PIECE, the transport command that fetches part of an answer too
 * long for the reply slot (docs/protocol.md, "Long commands"): the executor
 * kept the answer, and this hands on as much of it from a u64 offset as the
 * slot holds, behind the answer's u64 length. */
#ifndef AV_ANSWER_PIECE_COMMAND_HPP
#define AV_ANSWER_PIECE_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextExecutor;

class AnswerPieceCommand {
public:
    static constexpr size_t body_size = 8u;
    static constexpr size_t length_size = 8u;

    /* `body` is the header's size less the header's bytes, as the stream framed it. */
    AnswerPieceCommand(ContextExecutor &executor, const CommandHeader &header, const uint8_t *body);

    /* Worker. malformed for a body of any length but eight, and for one
     * that asks no answer or has no room for the length; ok otherwise, with
     * `produced` the length's bytes and the part's. */
    [[nodiscard]] Status run(uint8_t *reply, size_t capacity, size_t &produced);

private:
    ContextExecutor &executor_;
    CommandHeader    header_;
    const uint8_t   *body_;
};

}  // namespace av

#endif  // AV_ANSWER_PIECE_COMMAND_HPP
