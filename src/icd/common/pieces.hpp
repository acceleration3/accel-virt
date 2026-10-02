/* A run of `total` units cut into pieces one command can carry: at most
 * `most` each, in order. What every API sends a long run with - a buffer's
 * bytes, a list of rectangles, barriers or command lists - when one command
 * has a fixed room. The first piece is the one a write may discard with. */
#ifndef AV_ICD_PIECES_HPP
#define AV_ICD_PIECES_HPP

#include <stdint.h>

namespace av::icd {

class Pieces {
public:
    /* `most` is at least one unit; none sends nothing. */
    Pieces(uint64_t total, uint64_t most);

    /* On to the next piece; false once every unit has been. */
    [[nodiscard]] bool next();
    /* The piece's first unit, from the run's start, and how many it has. */
    uint64_t offset() const;
    uint64_t count() const;
    bool     first() const;

private:
    uint64_t total_;
    uint64_t most_;
    uint64_t offset_;
    uint64_t count_;
};

}  // namespace av::icd

#endif  // AV_ICD_PIECES_HPP
