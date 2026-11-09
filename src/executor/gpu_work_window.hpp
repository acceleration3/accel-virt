#ifndef AV_GPU_WORK_WINDOW_HPP
#define AV_GPU_WORK_WINDOW_HPP

#include <stdint.h>

namespace av {

class IdleQueue;

class GpuWorkWindow {
public:
    static constexpr uint64_t window_bytes = uint64_t{256} << 20u;
    static constexpr uint64_t wait_nanoseconds = 2000000000u;

    GpuWorkWindow();

    void     add(uint64_t bytes);
    bool     full() const;
    uint64_t pending() const;
    uint64_t roll(uint64_t ticket);
    bool     settle(IdleQueue &queue);
    void     forget();

    bool invariants_hold() const;

private:
    uint64_t gathered_;
    uint64_t previous_;
};

}

#endif
