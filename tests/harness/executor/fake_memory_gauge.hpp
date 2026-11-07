#ifndef AV_FAKE_MEMORY_GAUGE_HPP
#define AV_FAKE_MEMORY_GAUGE_HPP

#include "executor/host_memory_gauge.hpp"

#include <cstdint>

namespace av {

class FakeMemoryGauge final : public HostMemoryGauge {
public:
    std::uint64_t now_ms() override { return now; }
    bool          available(std::uint64_t &bytes) override
    {
        reads++;
        if (!readable) return false;
        bytes = free_bytes;
        return true;
    }
    void relieve() override { relieved++; }

    void set(std::uint64_t bytes, std::uint64_t later)
    {
        free_bytes = bytes;
        now += later;
    }

    std::uint64_t now = 1000u;
    std::uint64_t free_bytes = UINT64_MAX;
    bool          readable = true;
    std::uint32_t reads = 0u;
    std::uint32_t relieved = 0u;
};

}

#endif
