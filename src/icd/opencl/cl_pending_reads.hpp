#ifndef AV_CLICD_CL_PENDING_READS_HPP
#define AV_CLICD_CL_PENDING_READS_HPP

#include "icd/opencl/cl_pending_read.hpp"

#include <stdint.h>

namespace av::clicd {

struct ClPendingReadsProbe;

class ClPendingReads {
public:
    static constexpr uint32_t max_reads = 64u;

    ClPendingReads();
    ClPendingReads(const ClPendingReads &) = delete;
    ClPendingReads &operator=(const ClPendingReads &) = delete;

    [[nodiscard]] bool record(uint64_t event, uint64_t queue, uint64_t buffer, uint64_t offset,
                              uint64_t bytes, void *into, bool ours, uint64_t scratch = 0u);
    ClPendingRead *waiting_on(uint64_t event);
    const ClPendingRead *at(uint32_t slot) const;
    uint32_t       forget_queue(uint64_t queue, uint64_t *markers, uint64_t *scratches = nullptr,
                                uint32_t *scratch_count = nullptr);

    bool     has_room() const;
    uint32_t outstanding() const;
    uint64_t delivered() const;
    void     count_delivery();

    bool invariants_hold() const;

private:
    friend struct ClPendingReadsProbe;

    ClPendingRead rows_[max_reads];
    uint32_t      held_;
    uint64_t      delivered_;
};

}

#endif
