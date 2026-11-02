#ifndef AV_GUEST_FRAMED_BLOCK_HPP
#define AV_GUEST_FRAMED_BLOCK_HPP

#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_writer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class CommandPieces;
class WireSpill;

struct FramedBlockProbe;

class FramedBlock {
public:
    static constexpr size_t header_bytes = CommandHeader::encoded_size;

    FramedBlock(uint8_t *block, size_t capacity);
    FramedBlock(const FramedBlock &) = delete;
    FramedBlock &operator=(const FramedBlock &) = delete;

    bool                 fits(size_t bytes) const;
    WireWriter           lend(size_t room);
    WireWriter           lend(size_t room, WireSpill &spill);
    [[nodiscard]] bool   frame(uint32_t command_id, const WireWriter &writer, uint8_t flags);
    [[nodiscard]] bool   frame_piece(CommandPieces &pieces);
    [[nodiscard]] bool   append(const uint8_t *bytes, size_t length);
    const uint8_t       *bytes() const;
    uint8_t             *bytes();
    size_t               used() const;
    size_t               capacity() const;
    void                 clear();
    void                 adopt(uint8_t *block, size_t capacity);

    bool invariants_hold() const;

private:
    friend struct FramedBlockProbe;

    uint8_t *block_;
    size_t   capacity_;
    size_t   used_;
};

}

#endif
