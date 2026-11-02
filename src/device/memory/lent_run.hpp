/* A run of a lending's memory that guest pages lie over, in the order a
 * present names them (GuestRamLendings::locate): which lending, where in its
 * dma-buf the first page starts, and how many bytes the pages are. */
#ifndef AV_LENT_RUN_HPP
#define AV_LENT_RUN_HPP

#include <stdint.h>

namespace av {

class GuestRamLending;

class LentRun {
public:
    LentRun();
    LentRun(GuestRamLending &lending, uint64_t offset, uint64_t length);

    /* Null for a run that is nowhere. */
    GuestRamLending *lending() const;
    uint64_t         offset() const;
    uint64_t         length() const;

private:
    GuestRamLending *lending_;
    uint64_t         offset_;
    uint64_t         length_;
};

}  // namespace av

#endif  // AV_LENT_RUN_HPP
