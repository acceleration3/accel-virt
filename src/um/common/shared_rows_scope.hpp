#ifndef AV_UM_SHARED_ROWS_SCOPE_HPP
#define AV_UM_SHARED_ROWS_SCOPE_HPP

#include <stdint.h>

namespace av {

class SharedRowsScope {
public:
    SharedRowsScope();
    ~SharedRowsScope();
    SharedRowsScope(const SharedRowsScope &) = delete;
    SharedRowsScope &operator=(const SharedRowsScope &) = delete;

private:
    using Withdraw = void (*)();

    void ask(void *module);

    static constexpr uint32_t max_asked = 8u;

    Withdraw withdraw_[max_asked];
    uint32_t asked_;
};

}

#endif
