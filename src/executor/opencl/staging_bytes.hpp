#ifndef AV_STAGING_BYTES_HPP
#define AV_STAGING_BYTES_HPP

#include "executor/vram_account.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ByteLedger;

class StagingBytes {
public:
    StagingBytes();
    ~StagingBytes();
    StagingBytes(const StagingBytes &) = delete;
    StagingBytes &operator=(const StagingBytes &) = delete;

    void               bind(ByteLedger *ledger);
    [[nodiscard]] bool reserve(size_t bytes);
    void               release();
    uint8_t           *bytes() const;
    size_t             held() const;

private:
    uint8_t    *bytes_;
    size_t      held_;
    VramAccount account_;
};

}

#endif
