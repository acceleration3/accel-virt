/* One command, framed as the pieces the device gathers back into it
 * (docs/protocol.md, "Long commands"): as many bytes of its payload as each
 * room takes, behind a piece's header, and the last of them behind the
 * command's own. A command whose payload fits the first room is that command
 * alone, so a producer frames every command it cannot be sure of this way. */
#ifndef AV_COMMAND_PIECES_HPP
#define AV_COMMAND_PIECES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class CommandPieces {
public:
    /* Less room than this past a header is not worth a piece of its own
     * when the producer can send what it has framed and empty its room. */
    static constexpr size_t least_piece = 4096u;

    /* Command `command_id`, asking with `flags`, with `length` bytes of
     * payload at `body`, which outlives the framing and lies apart from every
     * room it is framed into. */
    CommandPieces(uint32_t command_id, uint8_t flags, const uint8_t *body, size_t length);
    CommandPieces(const CommandPieces &) = delete;
    CommandPieces &operator=(const CommandPieces &) = delete;

    /* Frames the next piece at `into`, which has `room` bytes, and answers
     * how many it framed: the rest of the payload behind the command's own
     * header when it fits, otherwise a piece of what does - or 0, framing
     * nothing, when the room takes no byte of a piece. */
    size_t frame_next(uint8_t *into, size_t room);
    /* Whether the command itself has been framed. */
    bool   done() const;
    /* Payload bytes not yet framed. */
    size_t rest() const;
    /* Pieces framed before the command. */
    size_t pieces() const;

    bool invariants_hold() const;

private:
    uint32_t       command_id_;
    uint8_t        flags_;
    const uint8_t *body_;
    size_t         length_;
    size_t         framed_;
    size_t         pieces_;
    bool           done_;
};

}  // namespace av

#endif  // AV_COMMAND_PIECES_HPP
