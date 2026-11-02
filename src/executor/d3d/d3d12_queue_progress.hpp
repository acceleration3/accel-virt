#ifndef AV_D3D12_QUEUE_PROGRESS_HPP
#define AV_D3D12_QUEUE_PROGRESS_HPP

#include "core/support/growable_array.hpp"

#include <stdint.h>

struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12Fence;

namespace av {

class BlockSource;
class DriverLog;

class EventfdWake;
struct D3d12QueueProgressProbe;

class D3d12QueueProgress {
public:
    D3d12QueueProgress(EventfdWake &wake, BlockSource &blocks);
    D3d12QueueProgress(const D3d12QueueProgress &) = delete;
    D3d12QueueProgress &operator=(const D3d12QueueProgress &) = delete;

    [[nodiscard]] bool executed(uint32_t id, ID3D12CommandQueue *queue, ID3D12Device *device);
    [[nodiscard]] bool after_siblings(uint32_t id, ID3D12CommandQueue *queue) const;
    [[nodiscard]] bool mark_all();
    void               forget(uint32_t id);
    void               clear();
    void               log_stuck(DriverLog &lines) const;

    uint64_t marked() const;
    uint64_t reached() const;

    bool invariants_hold() const;

private:
    struct Queue {
        uint32_t            id_;
        ID3D12CommandQueue *queue_;
        ID3D12Fence        *fence_;
        uint64_t            signalled_;
    };

    uint32_t index_of(uint32_t id) const;

    EventfdWake         &wake_;
    GrowableArray<Queue> queues_;
    uint64_t             marked_;

    friend struct D3d12QueueProgressProbe;
};

}

#endif
