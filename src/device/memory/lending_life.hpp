/* Whether one lending still exists, readable from the present engine's
 * thread after the lending is gone (GuestRamLendings::watch): the lending
 * holds one reference and each present that will copy into it another, taken
 * and let go on the main loop, so the flag outlives the lending for as long as
 * a present needs to ask it. Set before the pages are given their RAM back,
 * so a present that sees it clear is copying into memory the guest still has. */
#ifndef AV_LENDING_LIFE_HPP
#define AV_LENDING_LIFE_HPP

#include <stdint.h>

namespace av {

class LendingLife {
public:
    LendingLife();
    LendingLife(const LendingLife &) = delete;
    LendingLife &operator=(const LendingLife &) = delete;

    /* Main loop. */
    void     give_back();
    void     hold();
    /* How many references remain. */
    uint32_t let_go();
    /* Any thread. */
    bool     given_back() const;

private:
    uint32_t refs_;
    uint32_t given_back_;
};

}  // namespace av

#endif  // AV_LENDING_LIFE_HPP
