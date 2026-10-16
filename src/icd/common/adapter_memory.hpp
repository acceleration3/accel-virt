/* One adapter's memory as this process may use it, read from Windows on
 * demand: the budget VidMm gives it of the GPU's own (local) memory and of the
 * system memory the GPU borrows (non-local), and what its devices use of each
 * now (D3DKMTQueryVideoMemoryInfo, what DXGI's QueryVideoMemoryInfo answers).
 * Every user-mode driver that tells an application how much memory is left
 * reads it here, so they all say the same: Vulkan's VK_EXT_memory_budget,
 * OpenCL's CL_DEVICE_GLOBAL_FREE_MEMORY_AMD and NVAPI's current available
 * memory. */
#ifndef AV_ICD_ADAPTER_MEMORY_HPP
#define AV_ICD_ADAPTER_MEMORY_HPP

#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"

#include <stdint.h>

namespace av {
class Kmt;
}  // namespace av

namespace av::icd {

class AdapterMemory {
public:
    /* `adapter` is a kernel adapter handle the owner keeps open while this
     * reads it. */
    AdapterMemory(Kmt &kmt, uint32_t adapter);
    AdapterMemory(const AdapterMemory &) = delete;
    AdapterMemory &operator=(const AdapterMemory &) = delete;

    /* Both groups' budget and usage, read now. The kernel's refusal of
     * either is the answer, and leaves every figure zero. */
    [[nodiscard]] Status read();
    uint64_t             budget(MemorySegmentGroup group) const;
    uint64_t             usage(MemorySegmentGroup group) const;
    /* What of the group's budget its usage leaves: none once usage passes
     * it, which VidMm allows when it trims the budget under a process. */
    uint64_t             available(MemorySegmentGroup group) const;

    /* Every figure is zero until a read is answered. */
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

}  // namespace av::icd

#endif  // AV_ICD_ADAPTER_MEMORY_HPP
