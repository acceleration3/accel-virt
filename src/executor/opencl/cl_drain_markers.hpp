#ifndef AV_CL_DRAIN_MARKERS_HPP
#define AV_CL_DRAIN_MARKERS_HPP

#include "core/support/keyed_slots.hpp"
#include "proto/generated/opencl/cl_dispatch_server.h"

#include <stdint.h>

namespace av {

class BlockSource;
class MonotonicClock;
class TeardownBudget;

class ClDrainMarkers {
public:
    explicit ClDrainMarkers(BlockSource &blocks);
    ~ClDrainMarkers();
    ClDrainMarkers(const ClDrainMarkers &) = delete;
    ClDrainMarkers &operator=(const ClDrainMarkers &) = delete;

    [[nodiscard]] bool settle(cl_command_queue queue, uint64_t work, MonotonicClock &clock, TeardownBudget &budget);
    void               release_all();

    uint32_t count() const;

private:
    struct Row {
        void    *event_;
        uint64_t work_;
    };

    bool reached(const Row &row) const;

    KeyedSlots<uint64_t, Row> rows_;
};

}

#endif
