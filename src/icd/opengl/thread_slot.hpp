/* The calling thread's own state, which WGL keeps per thread (feature 050,
 * research.md §3): the one value the ICD stores for it - its current context -
 * its identity, and the error WGL reports through GetLastError. A freestanding
 * DLL has no implicit thread-local storage, so the Windows shim answers through
 * TlsGetValue and friends; a test switches the calling thread by name. */
#ifndef AV_GLICD_THREAD_SLOT_HPP
#define AV_GLICD_THREAD_SLOT_HPP

#include <stdint.h>

namespace av::glicd {

class ThreadSlot {
public:
    ThreadSlot(const ThreadSlot &) = delete;
    ThreadSlot &operator=(const ThreadSlot &) = delete;

    /* Null until this thread set a value. */
    virtual void    *get() = 0;
    virtual void     set(void *value) = 0;
    /* Never 0. */
    virtual uint32_t thread_id() = 0;
    /* What GetLastError answers this thread next. */
    virtual void     set_last_error(uint32_t code) = 0;

protected:
    ThreadSlot() = default;
    ~ThreadSlot() = default;
};

}  // namespace av::glicd

#endif  // AV_GLICD_THREAD_SLOT_HPP
