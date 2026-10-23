#ifndef AV_ICD_CALLING_THREAD_HPP
#define AV_ICD_CALLING_THREAD_HPP

#include <stdint.h>

namespace av::icd {

class CallingThread {
public:
    CallingThread(const CallingThread &) = delete;
    CallingThread &operator=(const CallingThread &) = delete;

    // Nonzero and stable for the lifetime of the calling thread.
    virtual uint32_t thread_id() = 0;

protected:
    CallingThread() = default;
    ~CallingThread() = default;
};

}  // namespace av::icd

#endif  // AV_ICD_CALLING_THREAD_HPP
