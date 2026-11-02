#ifndef AV_ICD_ADAPTER_MEMORY_HPP
#define AV_ICD_ADAPTER_MEMORY_HPP

#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"

#include <stdint.h>

namespace av {
class Kmt;
}

namespace av::icd {

class AdapterMemory {
public:
    AdapterMemory(Kmt &kmt, uint32_t adapter);
    AdapterMemory(const AdapterMemory &) = delete;
    AdapterMemory &operator=(const AdapterMemory &) = delete;

    [[nodiscard]] Status read();
    uint64_t             budget(MemorySegmentGroup group) const;
    uint64_t             usage(MemorySegmentGroup group) const;
    uint64_t             available(MemorySegmentGroup group) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t groups = 2u;

    void clear();

    Kmt     &kmt_;
    uint32_t adapter_;
    bool     read_;
    uint64_t budgets_[groups];
    uint64_t usages_[groups];
};

}

#endif
