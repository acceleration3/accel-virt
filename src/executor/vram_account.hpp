#ifndef AV_VRAM_ACCOUNT_HPP
#define AV_VRAM_ACCOUNT_HPP

#include <stdint.h>

namespace av {

class ByteLedger;

class VramAccount {
public:
    VramAccount();
    explicit VramAccount(ByteLedger &ledger);
    VramAccount(const VramAccount &) = delete;
    VramAccount &operator=(const VramAccount &) = delete;

    void               bind(ByteLedger *ledger);
    bool               bound() const;
    bool               bound_to(const ByteLedger *ledger) const;
    [[nodiscard]] bool take(uint64_t bytes);
    void               give(uint64_t bytes);
    uint64_t           close();

    uint64_t held() const;
    uint64_t cap() const;
    uint64_t free_bytes() const;
    uint64_t used() const;

private:
    ByteLedger *ledger_;
    uint64_t    held_;
};

}

#endif
