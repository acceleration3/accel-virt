#ifndef AV_GLICD_THREAD_SLOT_HPP
#define AV_GLICD_THREAD_SLOT_HPP

#include "icd/common/calling_thread.hpp"

#include <stdint.h>

namespace av::glicd {

class ThreadSlot : public icd::CallingThread {
public:
    ThreadSlot(const ThreadSlot &) = delete;
    ThreadSlot &operator=(const ThreadSlot &) = delete;

    virtual void    *get() = 0;
    virtual void     set(void *value) = 0;
    virtual void     set_last_error(uint32_t code) = 0;

protected:
    ThreadSlot() = default;
    ~ThreadSlot() = default;
};

}

#endif
