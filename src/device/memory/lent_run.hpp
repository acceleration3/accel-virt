#ifndef AV_LENT_RUN_HPP
#define AV_LENT_RUN_HPP

#include <stdint.h>

namespace av {

class GuestRamLending;

class LentRun {
public:
    LentRun();
    LentRun(GuestRamLending &lending, uint64_t offset, uint64_t length);

    GuestRamLending *lending() const;
    uint64_t         offset() const;
    uint64_t         length() const;

private:
    GuestRamLending *lending_;
    uint64_t         offset_;
    uint64_t         length_;
};

}

#endif
