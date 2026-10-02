/* A command's list of `total` entries - rectangles to clear or discard - in
 * pieces of at most `most`, where a list of none is itself one command: a
 * clear that names no rectangle clears the whole view. So unlike a run of
 * bytes (Pieces), an empty list is one piece of no entries, never no piece. */
#ifndef AV_ICD_LIST_PIECES_HPP
#define AV_ICD_LIST_PIECES_HPP

#include <stdint.h>

namespace av::icd {

class ListPieces {
public:
    /* `most` is at least one entry; none sends no command at all. */
    ListPieces(uint64_t total, uint64_t most);

    /* On to the next piece; false once every entry has been, and after the
     * one piece of an empty list. */
    [[nodiscard]] bool next();
    /* The piece's first entry, from the list's start, and how many it has. */
    uint64_t offset() const;
    uint64_t count() const;

private:
    uint64_t total_;
    uint64_t most_;
    uint64_t offset_;
    uint64_t count_;
    uint64_t sent_;
};

}  // namespace av::icd

#endif  // AV_ICD_LIST_PIECES_HPP
