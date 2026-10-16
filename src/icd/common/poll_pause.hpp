/* The pause between two asks of a wait whose host call answers at once (a
 * status poll rather than a blocking wait): none for the wait's first
 * millisecond - most such waits end inside it, and asking again is sooner than
 * any wake could come - then a brief sleep per round, so a long wait is not a
 * round trip every few microseconds and a processor taken from the
 * application. What OpenCL's event polls and Vulkan's query-pool waits pause
 * by; OpenCL sleeps on its driver's wake instead where it has one. */
#ifndef AV_ICD_POLL_PAUSE_HPP
#define AV_ICD_POLL_PAUSE_HPP

#include <stdint.h>

namespace av {
class Kmt;
}  // namespace av

namespace av::icd {

class PollPause {
public:
    /* How long a wait asks back to back: a second's fraction, 1 ms. */
    static constexpr uint64_t spin_per_second = 1000u;

    /* A wait starting now. */
    explicit PollPause(Kmt &kmt);
    PollPause(const PollPause &) = delete;
    PollPause &operator=(const PollPause &) = delete;

    /* Still within the first millisecond, on a counter that measures. */
    bool spinning() const;
    /* A brief sleep. */
    void rest();
    /* Nothing while spinning, a rest after. */
    void between();

private:
    Kmt     &kmt_;
    uint64_t frequency_;
    uint64_t started_;
};

}  // namespace av::icd

#endif  // AV_ICD_POLL_PAUSE_HPP
