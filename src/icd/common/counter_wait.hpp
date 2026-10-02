/* A wait with a timeout, asked of the host a slice at a time (SlicedWait) and
 * timed on the guest's own performance counter, read through the Kmt: what
 * every ICD wait that could outlast TdrDelay is - Vulkan's fence and semaphore
 * waits, OpenCL's blocking calls. */
#ifndef AV_ICD_COUNTER_WAIT_HPP
#define AV_ICD_COUNTER_WAIT_HPP

#include "icd/common/sliced_wait.hpp"

#include <stdint.h>

namespace av {
class Kmt;
}  // namespace av

namespace av::icd {

class CounterWait {
public:
    /* A wait of `timeout` nanoseconds, from now. */
    CounterWait(const Kmt &kmt, uint64_t timeout);
    CounterWait(const CounterWait &) = delete;
    CounterWait &operator=(const CounterWait &) = delete;

    /* Whether the host is to be asked again, with `slice` the timeout to
     * send: the first time always, later while the deadline has not passed. */
    [[nodiscard]] bool next(uint64_t &slice);

private:
    const Kmt &kmt_;
    uint64_t   frequency_;
    /* Read before the wait is made of it: the counter's own frequency is
     * written by the same read. */
    uint64_t   start_;
    SlicedWait wait_;
};

}  // namespace av::icd

#endif  // AV_ICD_COUNTER_WAIT_HPP
