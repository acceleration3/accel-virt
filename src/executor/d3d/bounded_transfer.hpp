#ifndef AV_BOUNDED_TRANSFER_HPP
#define AV_BOUNDED_TRANSFER_HPP

#include <stdint.h>

namespace av {

class BoundedTransfer {
public:
    static constexpr uint64_t most_bytes = uint64_t{64} << 20u;

    BoundedTransfer(uint64_t units, uint64_t unit_bytes);

    bool split() const;
    bool next(uint64_t &first, uint64_t &count);

    bool invariants_hold() const;

private:
    uint64_t units_;
    uint64_t per_piece_;
    uint64_t done_;
};

}

#endif
