#ifndef AV_COMMAND_PIECES_HPP
#define AV_COMMAND_PIECES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class CommandPieces {
public:
    static constexpr size_t least_piece = 4096u;

    CommandPieces(uint32_t command_id, uint8_t flags, const uint8_t *body, size_t length);
    CommandPieces(const CommandPieces &) = delete;
    CommandPieces &operator=(const CommandPieces &) = delete;

    size_t frame_next(uint8_t *into, size_t room);
    bool   done() const;
    size_t rest() const;
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

}

#endif
