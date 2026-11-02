#ifndef AV_TRACE_HOST_HPP
#define AV_TRACE_HOST_HPP

#include <stdint.h>

namespace av {

class TraceHost {
public:
    TraceHost() = default;
    TraceHost(const TraceHost &) = delete;
    TraceHost &operator=(const TraceHost &) = delete;

    virtual uint64_t thread_key() const = 0;
    virtual uint32_t thread_number() const = 0;
    virtual uint64_t clock() const = 0;
    virtual uint16_t level() const = 0;
    virtual bool     exchange(uint64_t &word, uint64_t expected, uint64_t desired) = 0;

protected:
    ~TraceHost() = default;
};

}

#endif
