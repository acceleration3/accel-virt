/* A buffer write cut into pieces a command can carry: `length` bytes in
 * pieces of at most `most`, the first of which alone may discard what the
 * buffer held. Direct3D 9's WriteBuffer9 and 11's UpdateSubresource of a
 * buffer each send one per piece. */
#ifndef AV_D3D_BYTE_PIECES_HPP
#define AV_D3D_BYTE_PIECES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class BytePieces {
public:
    /* `most` is at least one byte. */
    BytePieces(uint64_t length, uint64_t most);

    /* On to the next piece; false once every byte has been. */
    [[nodiscard]] bool next();
    /* The piece's first byte, from the write's start, and its bytes. */
    uint64_t offset() const;
    uint64_t bytes() const;
    bool     first() const;

private:
    uint64_t length_;
    uint64_t most_;
    uint64_t offset_;
    uint64_t bytes_;
};

}  // namespace av::d3d

#endif  // AV_D3D_BYTE_PIECES_HPP
