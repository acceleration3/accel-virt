#ifndef AV_VRAM_BUDGET_SOURCE_HPP
#define AV_VRAM_BUDGET_SOURCE_HPP

#include <stdint.h>

namespace av {

class VramBudgetSource {
public:
    VramBudgetSource(const VramBudgetSource &) = delete;
    VramBudgetSource &operator=(const VramBudgetSource &) = delete;

    virtual bool device_local(uint64_t &budget, uint64_t &usage) = 0;

protected:
    VramBudgetSource() = default;
    ~VramBudgetSource() = default;
};

}

#endif
