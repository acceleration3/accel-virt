#ifndef AV_BYTE_LEDGER_HPP
#define AV_BYTE_LEDGER_HPP

#include <stdint.h>

namespace av {

class ByteLedger {
public:
    ByteLedger();
    ByteLedger(const ByteLedger &) = delete;
    ByteLedger &operator=(const ByteLedger &) = delete;

    [[nodiscard]] virtual bool take(uint64_t bytes);
    [[nodiscard]] virtual bool take_session(uint64_t bytes);
    void               give(uint64_t bytes);

    uint64_t cap() const;
    uint64_t used() const;
    uint64_t free_bytes() const;
    uint64_t refusals() const;

protected:
    ~ByteLedger() = default;

    void set_cap(uint64_t cap);
    void refuse();

private:
    uint64_t cap_;
    uint64_t used_;
    uint64_t refusals_;
};

}

#endif
