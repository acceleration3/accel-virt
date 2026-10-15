#include "proto/submission/command_pieces.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/submission/command_header.hpp"

namespace av {

namespace {

/* The most payload one header can carry. */
constexpr size_t most_body = CommandHeader::max_size - CommandHeader::encoded_size;

}  // namespace

CommandPieces::CommandPieces(uint32_t command_id, uint8_t flags, const uint8_t *body, size_t length)
    : command_id_(command_id), flags_(flags), body_(body), length_(length), framed_(0u), pieces_(0u),
      done_(false)
{
    AV_SCOPE("CommandPieces::CommandPieces");
}

size_t CommandPieces::frame_next(uint8_t *into, size_t room)
{
    AV_SCOPE("CommandPieces::frame_next");
    if (done_ || into == nullptr || room < CommandHeader::encoded_size) return 0u;
    const size_t rest = length_ - framed_;
    size_t       fits = room - CommandHeader::encoded_size;
    if (fits > most_body) fits = most_body;
    const bool last = rest <= fits;
    if (!last && fits == 0u) return 0u;
    const size_t   body = last ? rest : fits;
    const uint32_t id = last ? command_id_ : CommandHeader::piece_id;
    const uint8_t  flags = last ? flags_ : 0u;
    const uint32_t size = static_cast<uint32_t>(CommandHeader::encoded_size + body);
    /* The size is at most max_size and the flags are the caller's or none. */
    static_cast<void>(CommandHeader(id, size, flags).encode(into, CommandHeader::encoded_size));
    if (body != 0u) __builtin_memcpy(into + CommandHeader::encoded_size, body_ + framed_, body);
    framed_ += body;
    if (last) done_ = true;
    else pieces_++;
    return CommandHeader::encoded_size + body;
}

bool CommandPieces::done() const
{
    AV_SCOPE("CommandPieces::done");
    return done_;
}

size_t CommandPieces::rest() const
{
    AV_SCOPE("CommandPieces::rest");
    return length_ - framed_;
}

size_t CommandPieces::pieces() const
{
    AV_SCOPE("CommandPieces::pieces");
    return pieces_;
}

bool CommandPieces::invariants_hold() const
{
    AV_SCOPE("CommandPieces::invariants_hold");
    return framed_ <= length_ && (!done_ || framed_ == length_) && (body_ != nullptr || length_ == 0u);
}

}  // namespace av
