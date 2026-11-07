#ifndef AV_SUBMISSION_WATCHDOG_HPP
#define AV_SUBMISSION_WATCHDOG_HPP

#include <stdint.h>

namespace av {

class SubmissionWatchdog {
public:
    static constexpr uint64_t default_limit_ns = 10000000000ull;

    explicit SubmissionWatchdog(uint64_t limit_ns);
    SubmissionWatchdog(const SubmissionWatchdog &) = delete;
    SubmissionWatchdog &operator=(const SubmissionWatchdog &) = delete;

    void     waiting_at(uint64_t reached, uint64_t device_completions, uint64_t now);
    void     nothing_outstanding();
    bool     overran(uint64_t now) const;
    uint64_t limit() const;
    uint64_t deadline() const;

private:
    uint64_t limit_ns_;
    uint64_t since_ns_;
    uint64_t reached_;
    uint64_t device_completions_;
};

}

#endif
