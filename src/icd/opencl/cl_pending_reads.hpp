/* The non-blocking reads one process has outstanding.
 *
 * A read is recorded when it is queued and delivered when the ICD sees its
 * event complete - at a wait, at a finish, when the application asks the
 * event's status, or just before a callback on it runs. Delivery is the fetch
 * the enqueue deferred, and by then the host answers it at once.
 *
 * The bound is the point: an application may queue reads faster than it waits
 * for them, and this must not grow. Past it a read is made blocking instead,
 * which is slower and always correct - never dropped, because a read the
 * application was told succeeded has to deliver. */
#ifndef AV_CLICD_CL_PENDING_READS_HPP
#define AV_CLICD_CL_PENDING_READS_HPP

#include "icd/opencl/cl_pending_read.hpp"

#include <stdint.h>

namespace av::clicd {

struct ClPendingReadsProbe;

class ClPendingReads {
public:
    /* Reads one process may have outstanding at once. */
    static constexpr uint32_t max_reads = 64u;

    ClPendingReads();
    ClPendingReads(const ClPendingReads &) = delete;
    ClPendingReads &operator=(const ClPendingReads &) = delete;

    /* Remembers one. False when there is no room or the row is not a read,
     * and the caller is what makes the read blocking instead. */
    [[nodiscard]] bool record(uint64_t event, uint64_t queue, uint64_t buffer, uint64_t offset,
                              uint64_t bytes, void *into, bool ours);
    /* The read waiting on `event`, or null. */
    ClPendingRead *waiting_on(uint64_t event);
    /* One row by slot, for a caller walking the table. */
    const ClPendingRead *at(uint32_t slot) const;
    /* Forgets everything queued on `queue`: what a queue being released means
     * for reads nobody will ever wait for. `markers` takes the events the ICD
     * made for itself, up to max_reads of them, so the caller can give them
     * back; its count is the answer. */
    uint32_t       forget_queue(uint64_t queue, uint64_t *markers);

    uint32_t outstanding() const;
    /* Reads delivered since the ICD started, for a state dump and the tests. */
    uint64_t delivered() const;
    void     count_delivery();

    bool invariants_hold() const;

private:
    friend struct ClPendingReadsProbe;

    ClPendingRead rows_[max_reads];
    uint32_t      held_;
    uint64_t      delivered_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_PENDING_READS_HPP
