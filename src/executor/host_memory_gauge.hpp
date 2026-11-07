#ifndef AV_HOST_MEMORY_GAUGE_HPP
#define AV_HOST_MEMORY_GAUGE_HPP

#include <stdint.h>

namespace av {

class HostMemoryGauge {
public:
    virtual ~HostMemoryGauge() = default;

    virtual uint64_t now_ms() = 0;
    virtual bool     available(uint64_t &bytes) = 0;
    virtual void     relieve() = 0;
};

}

#endif
