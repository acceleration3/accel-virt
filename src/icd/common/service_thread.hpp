#ifndef AV_ICD_SERVICE_THREAD_HPP
#define AV_ICD_SERVICE_THREAD_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av::icd {
class ServiceLoop {
public:
    ServiceLoop(const ServiceLoop &) = delete;
    ServiceLoop &operator=(const ServiceLoop &) = delete;

    [[nodiscard]] virtual bool turn() = 0;

protected:
    ServiceLoop() = default;
    ~ServiceLoop() = default;
};

class ServiceThread {
public:
    ServiceThread(const ServiceThread &) = delete;
    ServiceThread &operator=(const ServiceThread &) = delete;

    [[nodiscard]] virtual Status start(ServiceLoop &loop) = 0;
    virtual void   stop() = 0;
    virtual bool   running() const = 0;
    virtual void   rest() = 0;
    virtual void   pause(bool driven) = 0;
    virtual void   wake() = 0;

    virtual uint64_t wake_handle() const = 0;
    virtual uint64_t woken() const = 0;
    virtual void     await_woken(uint64_t seen, uint64_t timeout) = 0;

protected:
    ServiceThread() = default;
    ~ServiceThread() = default;
};

}

#endif
