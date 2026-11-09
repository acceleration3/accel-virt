#include "executor/gpu_work_window.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/idle_queue.hpp"

namespace av {

GpuWorkWindow::GpuWorkWindow() : gathered_(0u), previous_(0u)
{
    AV_SCOPE("GpuWorkWindow::GpuWorkWindow");
}

void GpuWorkWindow::add(uint64_t bytes)
{
    AV_SCOPE("GpuWorkWindow::add");
    gathered_ = bytes > UINT64_MAX - gathered_ ? UINT64_MAX : gathered_ + bytes;
}

bool GpuWorkWindow::full() const
{
    AV_SCOPE("GpuWorkWindow::full");
    return gathered_ >= window_bytes;
}

uint64_t GpuWorkWindow::pending() const
{
    AV_SCOPE("GpuWorkWindow::pending");
    return gathered_;
}

uint64_t GpuWorkWindow::roll(uint64_t ticket)
{
    AV_SCOPE("GpuWorkWindow::roll");
    const uint64_t before = previous_;
    previous_ = ticket;
    gathered_ = 0u;
    return before;
}

bool GpuWorkWindow::settle(IdleQueue &queue)
{
    AV_SCOPE("GpuWorkWindow::settle");
    uint64_t ticket = 0u;
    if (!queue.mark_idle(ticket)) {
        gathered_ = 0u;
        return false;
    }
    const uint64_t before = roll(ticket);
    return before == 0u || queue.reached(before, wait_nanoseconds);
}

void GpuWorkWindow::forget()
{
    AV_SCOPE("GpuWorkWindow::forget");
    gathered_ = 0u;
    previous_ = 0u;
}

bool GpuWorkWindow::invariants_hold() const
{
    AV_SCOPE("GpuWorkWindow::invariants_hold");
    return true;
}

}
