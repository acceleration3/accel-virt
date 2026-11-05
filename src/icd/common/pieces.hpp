#ifndef AV_ICD_PIECES_HPP
#define AV_ICD_PIECES_HPP

#include "icd/common/split_report.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::icd {

class Pieces {
public:
    Pieces(uint64_t total, uint64_t most);
    Pieces(uint64_t total, uint64_t most, ClientTransport &wire);

    [[nodiscard]] bool next();
    uint64_t offset() const;
    uint64_t count() const;
    bool     first() const;

private:
    uint64_t total_;
    uint64_t most_;
    uint64_t offset_;
    uint64_t count_;
    SplitReport report_;
};

}

#endif
