#ifndef AV_PROC_MEMORY_GAUGE_HPP
#define AV_PROC_MEMORY_GAUGE_HPP

#include "executor/host_memory_gauge.hpp"

#include <stdint.h>

namespace av {

class ProcMemoryGauge final : public HostMemoryGauge {
public:
    ProcMemoryGauge();
    ~ProcMemoryGauge() override;
    ProcMemoryGauge(const ProcMemoryGauge &) = delete;
    ProcMemoryGauge &operator=(const ProcMemoryGauge &) = delete;

    uint64_t now_ms() override;
    bool     available(uint64_t &bytes) override;
    void     relieve() override;

private:
    int fd_;
};

}

#endif
