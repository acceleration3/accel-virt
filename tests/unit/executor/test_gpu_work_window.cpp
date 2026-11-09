#include "executor/d3d/idle_queue.hpp"
#include "executor/gpu_work_window.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::GpuWorkWindow;

namespace {

class SlowQueue final : public av::IdleQueue {
public:
    SlowQueue() : issued_(0u), completed_(0u), waited_(0u) {}

    bool mark_idle(std::uint64_t &ticket) override
    {
        ticket = ++issued_;
        return true;
    }

    bool reached(std::uint64_t ticket, std::uint64_t) const override
    {
        const_cast<SlowQueue *>(this)->waited_++;
        const_cast<SlowQueue *>(this)->completed_ = ticket;
        return true;
    }

    std::uint64_t issued_;
    std::uint64_t completed_;
    std::uint64_t waited_;
};

}

TEST(gpu_work_window, closes_after_a_window_of_bytes)
{ GpuWorkWindow window;
  window.add(GpuWorkWindow::window_bytes - 1u);
  CHECK(!window.full());
  window.add(1u);
  CHECK(window.full()); }

TEST(gpu_work_window, a_guest_that_never_waits_leaves_at_most_two_windows_behind)
{ GpuWorkWindow window;
  SlowQueue     queue;
  std::uint64_t most_behind = 0u;
  for (int copy = 0; copy < 5000; copy++) {
      window.add(std::uint64_t{64} << 20u);
      if (window.full()) CHECK(window.settle(queue));
      const std::uint64_t behind = (queue.issued_ - queue.completed_) * GpuWorkWindow::window_bytes + window.pending();
      if (behind > most_behind) most_behind = behind;
  }
  CHECK(most_behind <= 2u * GpuWorkWindow::window_bytes + (std::uint64_t{64} << 20u));
  CHECK(queue.issued_ > 1000u); }

TEST(gpu_work_window, the_first_window_waits_for_nothing)
{ GpuWorkWindow window;
  SlowQueue     queue;
  window.add(GpuWorkWindow::window_bytes);
  CHECK(window.settle(queue));
  CHECK_EQ(queue.waited_, 0u);
  window.add(GpuWorkWindow::window_bytes);
  CHECK(window.settle(queue));
  CHECK_EQ(queue.waited_, 1u); }

TEST(gpu_work_window, forgetting_starts_over)
{ GpuWorkWindow window;
  window.add(GpuWorkWindow::window_bytes);
  window.forget();
  CHECK(!window.full());
  CHECK_EQ(window.pending(), 0u); }
