#ifndef AV_ICD_THREAD_WAKE_HPP
#define AV_ICD_THREAD_WAKE_HPP

#include <stdint.h>

namespace av::icd {

class ThreadWake {
public:
    ThreadWake(const ThreadWake &) = delete;
    ThreadWake &operator=(const ThreadWake &) = delete;

    virtual void raise() = 0;
    virtual bool wait(uint32_t milliseconds) = 0;

protected:
    ThreadWake() = default;
    ~ThreadWake() = default;
};

}

#endif
