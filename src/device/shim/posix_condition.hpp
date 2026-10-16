/* A pthread condition over one PosixLock, waited on with that lock held: the
 * lock is let go for the wait and held again before it returns. Deadlines are
 * on CLOCK_MONOTONIC (MonotonicClock::ahead gives one), so a wall clock set
 * back cannot stretch a wait. A waiting thread's trace slot shows it blocked
 * on this condition and holding nothing; woken, it holds the lock again.
 * Translation only. */
#ifndef AV_POSIX_CONDITION_HPP
#define AV_POSIX_CONDITION_HPP

#include <pthread.h>
#include <time.h>

namespace av {

class PosixLock;

class PosixCondition {
public:
    explicit PosixCondition(PosixLock &lock);
    /* No thread may be waiting: destroying a condition with a waiter never
     * returns. */
    ~PosixCondition();
    PosixCondition(const PosixCondition &) = delete;
    PosixCondition &operator=(const PosixCondition &) = delete;

    /* The lock held. Woken by signal or broadcast - or for no reason, so the
     * caller looks at what it waits for again. */
    void               wait();
    /* The same, giving up at `deadline`: false once it has passed. */
    [[nodiscard]] bool wait_until(const timespec &deadline);
    /* One waiter woken; every waiter. Either with the lock held or not. */
    void               signal();
    void               broadcast();

private:
    PosixLock     &lock_;
    pthread_cond_t condition_;
};

}  // namespace av

#endif  // AV_POSIX_CONDITION_HPP
