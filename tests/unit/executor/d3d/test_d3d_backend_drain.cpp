#include "core/log/driver_log.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/executor_log.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "harness/executor/generous_budget.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <atomic>
#include <pthread.h>
#include <time.h>

namespace av {
struct D3dBackendProbe {
    static void use(D3dBackend &backend, D3d11HostDevice *device) { backend.device11_ = device; }
};
}

using av::D3dBackend;
using av::D3dBackendProbe;
using av::D3d11HostDevice;

namespace {

av::RecordingLog sink;
av::ExecutorLog  executor_log(sink);

struct Drainer {
    D3dBackend        *backend;
    std::atomic<bool>  done;
    std::atomic<bool>  drained;
};

void *drain_it(void *context)
{
    Drainer &drainer = *static_cast<Drainer *>(context);
    av::GenerousBudget budget;
    drainer.drained.store(drainer.backend->drain(budget.budget()));
    drainer.done.store(true);
    return nullptr;
}

void wait_a_while()
{
    const timespec pause = {0, 150 * 1000 * 1000};
    nanosleep(&pause, nullptr);
}

}

TEST(d3d_backend_drain, returns_at_once_for_a_backend_with_no_device)
{ D3dBackend backend(executor_log);
  av::GenerousBudget budget;
  CHECK(backend.drain(budget.budget()));
  CHECK(backend.invariants_hold()); }

TEST(d3d_backend_drain, a_session_that_never_made_a_direct3d11_device_neither_takes_the_device_lock_nor_waits)
{ D3dBackend backend(executor_log);
  D3d11HostDevice device(executor_log.d3d11());
  D3dBackendProbe::use(backend, &device);
  Drainer drainer = {&backend, false, false};
  device.lock(nullptr);
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, drain_it, &drainer), 0);
  wait_a_while();
  const bool finished = drainer.done.load();
  device.unlock();
  CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK(finished);
  CHECK(drainer.drained.load());
  D3dBackendProbe::use(backend, nullptr); }
