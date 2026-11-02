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

    AnswerPieceCommand(ContextExecutor &executor, const CommandHeader &header, const uint8_t *body);

    [[nodiscard]] Status run(uint8_t *reply, size_t capacity, size_t &produced);

private:
    ContextExecutor &executor_;
    CommandHeader    header_;
    const uint8_t   *body_;
};

}

#endif
