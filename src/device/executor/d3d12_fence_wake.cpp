#include "device/executor/d3d12_fence_wake.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace av {

namespace {

void *run_d3d12_fence_wake(void *wake)
{
    AV_SCOPE("run_d3d12_fence_wake");
    static_cast<D3d12FenceWake *>(wake)->run();
    return nullptr;
}

}  // namespace

D3d12FenceWake::D3d12FenceWake()
    : signal_(nullptr), context_(nullptr), reached_fd_(-1), stop_fd_(-1), thread_{}, running_(false)
{
    AV_SCOPE("D3d12FenceWake::D3d12FenceWake");
}

D3d12FenceWake::~D3d12FenceWake()
{
    AV_SCOPE("D3d12FenceWake::~D3d12FenceWake");
    stop();
}

void D3d12FenceWake::bind(void (*signal)(void *context), void *context)
{
    AV_SCOPE("D3d12FenceWake::bind");
    signal_ = signal;
    context_ = context;
}

bool D3d12FenceWake::watch(ID3D12Fence *fence, uint64_t value)
{
    AV_SCOPE("D3d12FenceWake::watch");
    if (signal_ == nullptr) return true;
    if (!running_ && !start()) return false;
    const HANDLE event = reinterpret_cast<HANDLE>(static_cast<intptr_t>(reached_fd_));
    return SUCCEEDED(fence->SetEventOnCompletion(value, event));
}

bool D3d12FenceWake::start()
{
    AV_SCOPE("D3d12FenceWake::start");
    if (signal_ == nullptr) return false;
    reached_fd_ = eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
    stop_fd_ = eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
    /* vkd3d-proton reads descriptor 0 as no event at all. */
    running_ = reached_fd_ > 0 && stop_fd_ >= 0 && pthread_create(&thread_, nullptr, &run_d3d12_fence_wake, this) == 0;
    if (running_) return true;
    if (reached_fd_ >= 0) close(reached_fd_);
    if (stop_fd_ >= 0) close(stop_fd_);
    reached_fd_ = -1;
    stop_fd_ = -1;
    return false;
}

void D3d12FenceWake::stop()
{
    AV_SCOPE("D3d12FenceWake::stop");
    if (!running_) return;
    const uint64_t one = 1u;
    static_cast<void>(write(stop_fd_, &one, sizeof one));
    pthread_join(thread_, nullptr);
    close(reached_fd_);
    close(stop_fd_);
    reached_fd_ = -1;
    stop_fd_ = -1;
    running_ = false;
}

void D3d12FenceWake::run()
{
    AV_SCOPE("D3d12FenceWake::run");
    pollfd watched[2] = {{reached_fd_, POLLIN, 0}, {stop_fd_, POLLIN, 0}};
    for (;;) {
        if (poll(watched, 2u, -1) < 0) continue;
        if (watched[1].revents != 0) return;
        uint64_t count = 0u;
        if (read(reached_fd_, &count, sizeof count) == sizeof count) signal_(context_);
    }
}

}  // namespace av
