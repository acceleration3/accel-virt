#ifndef AV_ICD_LIST_PIECES_HPP
#define AV_ICD_LIST_PIECES_HPP

#include <stdint.h>

namespace av::icd {

class ListPieces {
public:
    ListPieces(uint64_t total, uint64_t most);

    [[nodiscard]] bool next();
    uint64_t offset() const;
    uint64_t count() const;

private:
    uint64_t total_;
    uint64_t most_;
    uint64_t offset_;
    uint64_t count_;
    uint64_t sent_;
};

}

#endif
