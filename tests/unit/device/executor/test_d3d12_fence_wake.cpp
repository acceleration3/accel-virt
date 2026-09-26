/* Waking a context's worker when the host GPU reaches a mark a queue
 * signalled. vkd3d-proton's part - writing the eventfd it is handed once its
 * fence reaches a value - is played by ScriptedFence, which keeps the
 * descriptor it was given and writes it when the test says the GPU got there;
 * everything else is the real thread, poll() and signal. */
#include "device/executor/d3d12_fence_wake.hpp"
#include "device/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"

#include <atomic>
#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>

using av::D3d12FenceWake;

namespace {

class ScriptedFence final : public ID3D12Fence {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE   AddRef() override { return 1u; }
    ULONG STDMETHODCALLTYPE   Release() override { return 1u; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(const WCHAR *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID, void **device) override
    {
        *device = nullptr;
        return E_NOTIMPL;
    }
    UINT64 STDMETHODCALLTYPE  GetCompletedValue() override { return completed; }
    HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64 value, HANDLE event) override
    {
        if (refuse) return E_FAIL;
        asked = value;
        descriptor = static_cast<int>(reinterpret_cast<std::intptr_t>(event));
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Signal(UINT64 value) override
    {
        completed = value;
        return S_OK;
    }

    /* The GPU reached the mark: vkd3d-proton writes the eventfd. */
    void reach()
    {
        const std::uint64_t one = 1u;
        static_cast<void>(write(descriptor, &one, sizeof one));
    }

    UINT64 completed = 0u;
    UINT64 asked = 0u;
    int    descriptor = -1;
    bool   refuse = false;
};

std::atomic<std::uint32_t> woken{0u};

void wake(void *context)
{
    static_cast<std::atomic<std::uint32_t> *>(context)->fetch_add(1u);
}

/* The wake runs on its own thread: wait, boundedly, for it to have woken. */
bool woken_reaches(std::uint32_t count)
{
    for (std::uint32_t tries = 0u; tries < 5000u; tries++) {
        if (woken.load() >= count) return true;
        const timespec pause = {0, 1000000};
        nanosleep(&pause, nullptr);
    }
    return false;
}

}  // namespace

/* Unbound there is no worker to wake: nothing is watched and nothing started,
 * which is still success for the caller. */
TEST(d3d12_fence_wake_watch, unbound_watches_nothing)
{ D3d12FenceWake wake;
  ScriptedFence fence;
  CHECK(wake.watch(&fence, 3u));
  CHECK_EQ(fence.descriptor, -1);
  wake.stop(); }

TEST(d3d12_fence_wake_watch, a_reached_mark_raises_the_signal_once_per_reach)
{ woken = 0u;
  D3d12FenceWake wake;
  wake.bind(&::wake, &woken);
  ScriptedFence fence;
  CHECK(wake.watch(&fence, 5u));
  CHECK(fence.asked == 5u);
  CHECK(fence.descriptor > 0);   /* never 0: vkd3d-proton reads 0 as no event */
  CHECK_EQ(woken.load(), 0u);
  fence.reach();
  CHECK(woken_reaches(1u));
  /* The same descriptor for the next mark: one thread, one eventfd. */
  const int first = fence.descriptor;
  CHECK(wake.watch(&fence, 9u));
  CHECK_EQ(fence.descriptor, first);
  fence.reach();
  CHECK(woken_reaches(2u));
  wake.stop();
  /* Stopped, the descriptor is closed and a later watch starts another thread. */
  CHECK_EQ(fcntl(first, F_GETFD), -1);
  CHECK(wake.watch(&fence, 11u));
  fence.reach();
  CHECK(woken_reaches(3u));
  wake.stop();
  wake.stop(); }

TEST(d3d12_fence_wake_watch, a_mark_vkd3d_proton_refuses_is_refused)
{ woken = 0u;
  D3d12FenceWake wake;
  wake.bind(&::wake, &woken);
  ScriptedFence fence;
  fence.refuse = true;
  CHECK(!wake.watch(&fence, 5u));
  fence.refuse = false;
  CHECK(wake.watch(&fence, 6u));
  CHECK(fence.asked == 6u);
  CHECK_EQ(woken.load(), 0u); }

/* The destructor stops a running thread: nothing outlives the wake. */
TEST(d3d12_fence_wake, a_running_wake_stops_with_its_owner)
{ woken = 0u;
  ScriptedFence fence;
  int descriptor = -1;
  {
      D3d12FenceWake wake;
      wake.bind(&::wake, &woken);
      CHECK(wake.watch(&fence, 1u));
      descriptor = fence.descriptor;
  }
  CHECK(descriptor > 0);
  CHECK_EQ(fcntl(descriptor, F_GETFD), -1); }
