#ifndef AV_CLICD_CL_HOST_NUMBERS_HPP
#define AV_CLICD_CL_HOST_NUMBERS_HPP

#include "core/support/growable_array.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::clicd {

class ClHostNumbers {
public:
    static constexpr uint32_t kept_inline = 64u;

    explicit ClHostNumbers(BlockSource &memory);
    ClHostNumbers(const ClHostNumbers &) = delete;
    ClHostNumbers &operator=(const ClHostNumbers &) = delete;

    uint64_t *room(uint32_t count);

    bool invariants_hold() const;

private:
    uint64_t                inline_[kept_inline];
    GrowableArray<uint64_t> spilled_;
};

}

#endif
