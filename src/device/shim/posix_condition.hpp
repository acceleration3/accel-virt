#ifndef AV_POSIX_CONDITION_HPP
#define AV_POSIX_CONDITION_HPP

#include <pthread.h>
#include <time.h>

namespace av {

class PosixLock;

class PosixCondition {
public:
    explicit PosixCondition(PosixLock &lock);
    ~PosixCondition();
    PosixCondition(const PosixCondition &) = delete;
    PosixCondition &operator=(const PosixCondition &) = delete;

    void               wait();
    [[nodiscard]] bool wait_until(const timespec &deadline);
    void               signal();
    void               broadcast();

private:
    PosixLock     &lock_;
    pthread_cond_t condition_;
};

}

#endif
