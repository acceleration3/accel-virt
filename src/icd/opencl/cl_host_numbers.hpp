/* The host's numbers for the objects one call names - a wait list, the
 * programs a link takes, the headers a compile does - however many the
 * application names. A short list lives in the object, so the calls that
 * name a few allocate nothing; a longer one is taken from the process's
 * memory for as long as the call. */
#ifndef AV_CLICD_CL_HOST_NUMBERS_HPP
#define AV_CLICD_CL_HOST_NUMBERS_HPP

#include "core/support/growable_array.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::clicd {

class ClHostNumbers {
public:
    /* Numbers kept in the object: the lists applications name. */
    static constexpr uint32_t kept_inline = 64u;

    explicit ClHostNumbers(BlockSource &memory);
    ClHostNumbers(const ClHostNumbers &) = delete;
    ClHostNumbers &operator=(const ClHostNumbers &) = delete;

    /* Room for `count` numbers, zeroed; null when there is no memory for a
     * list that long. */
    uint64_t *room(uint32_t count);

    bool invariants_hold() const;

private:
    uint64_t                inline_[kept_inline];
    GrowableArray<uint64_t> spilled_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_HOST_NUMBERS_HPP
