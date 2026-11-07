#ifndef AV_LOCK_RANK_HPP
#define AV_LOCK_RANK_HPP

#include <stdint.h>

namespace av {

enum class LockRank : uint16_t {
    doorbell_table = 70u,
    native_fence_table = 75u,
    hw_queue_table = 80u,
    context_table = 90u,
    device_table = 100u,
    av_control_requests = 105u,
    host_window = 107u,
    allocation_table = 110u,
    shared_objects = 115u,
    present_payload = 117u,
    present_queue = 118u,
    control_exchanges = 120u,
    dpc = 150u,
    av_context_slots = 190u,
    present_waiters = 195u,
    execution_node = 200u,
    hw_queue_contexts = 205u,
    hw_queue_submissions = 210u,
    scanout = 215u,
    control_queue = 220u,
    cursor_queue = 230u,
    av_context = 240u,
    log_budget = 250u,
    gpu_clock_origin = 260u,
};

}

#endif
