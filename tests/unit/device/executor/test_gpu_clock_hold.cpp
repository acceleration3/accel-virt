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
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
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

constexpr std::uint32_t openers = 8u;
constexpr std::uint32_t opening_rounds = 30u;
constexpr int           opening_deadline_ms = 30000;

struct Opener {
    GpuClockHold      *hold;
    pthread_barrier_t *start;
    std::int32_t       minor;
    bool               opened;
};

void *open_with_the_others(void *raw)
{
    Opener *const opener = static_cast<Opener *>(raw);
    pthread_barrier_wait(opener->start);
    opener->opened = opener->hold->open(opener->minor);
    return nullptr;
}

/* Rounds in which every opener's open returned true; a round where one never
 * returns never ends, which is what the parent's deadline catches. */
std::uint32_t open_together(std::int32_t minor)
{
    std::uint32_t whole = 0u;
    for (std::uint32_t round = 0u; round < opening_rounds; round++) {
        GpuClockHold      hold;
        pthread_barrier_t start;
        pthread_barrier_init(&start, nullptr, openers);
        Opener    racers[openers] = {};
        pthread_t threads[openers] = {};
        for (std::uint32_t index = 0u; index < openers; index++) {
            racers[index] = Opener{&hold, &start, minor, false};
            pthread_create(&threads[index], nullptr, &open_with_the_others, &racers[index]);
        }
        std::uint32_t opened = 0u;
        for (std::uint32_t index = 0u; index < openers; index++) {
            pthread_join(threads[index], nullptr);
            opened += racers[index].opened ? 1u : 0u;
        }
        pthread_barrier_destroy(&start);
        whole += opened == openers ? 1u : 0u;
    }
    return whole;
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

/* Two contexts on one GPU attach on workers of their own at the same moment,
 * and each opens the GPU's hold. Every open returns, true, round after round:
 * a second open that found the hold not yet open, and set up the condition the
 * first one's thread already waits on, never returned - and QEMU's main loop,
 * closing that context, waited on it for good. The rounds run in a child, so
 * one that never ends fails here instead of hanging the suite. */
TEST(gpu_clock_hold_open, eight_threads_opening_one_hold_at_once_all_return_true)
{ const std::int32_t minor = amdgpu_render_minor();
  if (minor < 0) return;
  int channel[2] = {-1, -1};
  CHECK_EQ(pipe(channel), 0);
  const pid_t child = fork();
  CHECK(child >= 0);
  if (child == 0) {
      const std::uint32_t whole = open_together(minor);
      static_cast<void>(write(channel[1], &whole, sizeof whole));
      _exit(0);
  }
  close(channel[1]);
  pollfd answer = {channel[0], POLLIN, 0};
  const bool answered = poll(&answer, 1u, opening_deadline_ms) == 1;
  std::uint32_t whole = 0u;
  const ssize_t got = answered ? read(channel[0], &whole, sizeof whole) : 0;
  close(channel[0]);
  if (!answered) kill(child, SIGKILL);
  int status = 0;
  CHECK_EQ(waitpid(child, &status, 0), child);
  CHECK(answered);
  CHECK_EQ(got, static_cast<ssize_t>(sizeof whole));
  CHECK_EQ(whole, opening_rounds); }
