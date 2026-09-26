/* The host GPU's clocks, held by a thread of their own while batches flow. The
 * render node is the host's, so what is judged on any host is every refusal -
 * no node, a node that is not there - and, where this host has an amdgpu
 * render node, that the hold opens once, stays open when asked again, is told
 * of batches without blocking the caller, and closes as often as asked. */
#include "device/executor/gpu_clock_hold.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>

using av::GpuClockHold;

namespace {

/* The minor of the first render node amdgpu drives, or -1. */
std::int32_t amdgpu_render_minor()
{
    for (std::int32_t minor = 128; minor < 192; minor++) {
        char link[96];
        std::snprintf(link, sizeof link, "/sys/class/drm/renderD%d/device/driver", minor);
        char target[256] = {};
        const ssize_t length = readlink(link, target, sizeof target - 1u);
        if (length <= 0) continue;
        const char *const name = std::strrchr(target, '/');
        if (name != nullptr && std::strcmp(name + 1, "amdgpu") == 0) return minor;
    }
    return -1;
}

std::uint64_t now_ns()
{
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::uint64_t>(now.tv_sec) * 1000000000u + static_cast<std::uint64_t>(now.tv_nsec);
}

}  // namespace

TEST(gpu_clock_hold_open, a_gpu_whose_driver_names_no_render_node_holds_nothing)
{ GpuClockHold hold;
  CHECK(!hold.open(-1));
  CHECK(!hold.open(-128));
  /* Never opened: told of a batch, and closed, it does nothing. */
  hold.busy();
  hold.close();
  hold.close(); }

TEST(gpu_clock_hold_open, a_render_node_that_is_not_there_holds_nothing)
{ GpuClockHold hold;
  CHECK(!hold.open(4095));
  hold.busy();
  CHECK(!hold.open(4095)); }

TEST(gpu_clock_hold_open, an_amdgpu_node_opens_once_and_is_told_of_batches_without_waiting)
{ const std::int32_t minor = amdgpu_render_minor();
  if (minor < 0) return;   /* no amdgpu here: the refusals above are what this host can judge */
  GpuClockHold hold;
  CHECK(hold.open(minor));
  CHECK(hold.open(minor));   /* already open: true at once */
  CHECK(hold.open(-1));      /* the node is not asked again */
  const std::uint64_t started = now_ns();
  for (std::uint32_t batch = 0u; batch < 1000u; batch++) hold.busy();
  /* A batch pays an atomic store, not a wait on the watching thread. */
  CHECK(now_ns() - started < 100000000u);
  hold.close();
  hold.close();
  /* Closed, it opens again. */
  CHECK(hold.open(minor));
  hold.busy();
  hold.close(); }

/* The destructor closes an open hold: its thread is joined, not left running
 * over freed memory. */
TEST(gpu_clock_hold, an_open_hold_goes_with_its_owner)
{ const std::int32_t minor = amdgpu_render_minor();
  if (minor < 0) return;
  for (std::uint32_t round = 0u; round < 3u; round++) {
      GpuClockHold hold;
      CHECK(hold.open(minor));
      hold.busy();
  } }
