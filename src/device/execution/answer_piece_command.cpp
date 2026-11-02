#include "device/execution/answer_piece_command.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/context_executor.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

AnswerPieceCommand::AnswerPieceCommand(ContextExecutor &executor, const CommandHeader &header, const uint8_t *body)
    : executor_(executor), header_(header), body_(body)
{
    AV_SCOPE("AnswerPieceCommand::AnswerPieceCommand");
}

Status AnswerPieceCommand::run(uint8_t *reply, size_t capacity, size_t &produced)
{
    AV_SCOPE("AnswerPieceCommand::run");
    produced = 0u;
    if (header_.size() != CommandHeader::encoded_size + body_size || !header_.generates_reply() ||
        reply == nullptr || capacity < length_size)
        return Status::malformed;
    WireReader     request(body_, body_size);
    const uint64_t offset = request.read_u64();
    uint64_t       length = 0u;
    const size_t   copied = executor_.answer_part(offset, reply + length_size, capacity - length_size, length);
    WireWriter     answer(reply, length_size);
    answer.write_u64(length);
    produced = length_size + copied;
    return Status::ok;
}

}
