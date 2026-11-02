#ifndef AV_LENDING_LIFE_HPP
#define AV_LENDING_LIFE_HPP

#include <stdint.h>

namespace av {

class LendingLife {
public:
    LendingLife();
    LendingLife(const LendingLife &) = delete;
    LendingLife &operator=(const LendingLife &) = delete;

    void     give_back();
    void     hold();
    uint32_t let_go();
    bool     given_back() const;

private:
    uint32_t refs_;
    uint32_t given_back_;
};

}

#endif
