#ifndef AV_GPU_CLOCK_HOLD_HPP
#define AV_GPU_CLOCK_HOLD_HPP

#include "device/shim/posix_condition.hpp"
#include "device/shim/posix_lock.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/render_node_driver.hpp"

#include <pthread.h>
#include <stdint.h>

namespace av {

class GpuClockHold {
public:
    GpuClockHold();
    explicit GpuClockHold(const char *class_directory);
    ~GpuClockHold();
    GpuClockHold(const GpuClockHold &) = delete;
    GpuClockHold &operator=(const GpuClockHold &) = delete;

    bool open(int32_t render_minor);
    void busy();
    void close();
    void watch();

private:
    static constexpr uint64_t release_after_ns = 250000000u;
    static constexpr uint64_t look_every_ns = 50000000u;
    static constexpr uint32_t slow_looks = 8u;
    static constexpr uint32_t busy_percent = 50u;
    static constexpr uint64_t probe_every_ns = 4000000000u;
    static constexpr uint64_t first_probe_ns = 500000000u;

    bool     start(int32_t render_minor);
    bool     request(uint32_t pstate);
    uint32_t sensor(uint32_t type) const;
    bool     slow() const;
    void     take();
    void     let_go();

    MonotonicClock   clock_;
    RenderNodeDriver drivers_;
    int32_t          declined_;
    int             fd_;
    uint32_t        context_;
    pthread_t       thread_;
    PosixLock       lock_;
    PosixCondition  wake_;
    uint64_t        last_busy_;
    uint64_t        held_since_;
    uint64_t        hold_for_;
    uint32_t        peak_mhz_;
    uint32_t        slow_;
    bool            held_;
    bool            wanted_;
    bool            stopping_;
    bool            open_;
};

}

#endif
