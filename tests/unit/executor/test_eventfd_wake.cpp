#include "executor/eventfd_wake.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"

#include <atomic>
#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>

using av::EventfdWake;

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

class ScriptedFence11 final : public ID3D11Fence {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE   AddRef() override { return 1u; }
    ULONG STDMETHODCALLTYPE   Release() override { return 1u; }
    void STDMETHODCALLTYPE    GetDevice(ID3D11Device **device) override { *device = nullptr; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateSharedHandle(const SECURITY_ATTRIBUTES *, DWORD, LPCWSTR, HANDLE *) override
    {
        return E_NOTIMPL;
    }
    UINT64 STDMETHODCALLTYPE  GetCompletedValue() override { return 0u; }
    HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64 value, HANDLE event) override
    {
        asked = value;
        descriptor = static_cast<int>(reinterpret_cast<std::intptr_t>(event));
        return S_OK;
    }

    void reach()
    {
        const std::uint64_t one = 1u;
        static_cast<void>(write(descriptor, &one, sizeof one));
    }

    UINT64 asked = 0u;
    int    descriptor = -1;
};

std::atomic<std::uint32_t> woken{0u};

void wake(void *context)
{
    static_cast<std::atomic<std::uint32_t> *>(context)->fetch_add(1u);
}

bool woken_reaches(std::uint32_t count)
{
    for (std::uint32_t tries = 0u; tries < 5000u; tries++) {
        if (woken.load() >= count) return true;
        const timespec pause = {0, 1000000};
        nanosleep(&pause, nullptr);
    }
    return false;
}

}

TEST(eventfd_wake_watch, unbound_watches_nothing)
{ EventfdWake wake;
  ScriptedFence fence;
  CHECK(wake.watch(fence, 3u));
  CHECK_EQ(fence.descriptor, -1);
  wake.stop(); }

TEST(eventfd_wake_watch, a_reached_mark_raises_the_signal_once_per_reach)
{ woken = 0u;
  EventfdWake wake;
  wake.bind(&::wake, &woken);
  ScriptedFence fence;
  CHECK(wake.watch(fence, 5u));
  CHECK(fence.asked == 5u);
  CHECK(fence.descriptor > 0);
  CHECK_EQ(woken.load(), 0u);
  fence.reach();
  CHECK(woken_reaches(1u));
  const int first = fence.descriptor;
  CHECK(wake.watch(fence, 9u));
  CHECK_EQ(fence.descriptor, first);
  fence.reach();
  CHECK(woken_reaches(2u));
  wake.stop();
  CHECK(fcntl(first, F_GETFD) != -1);
  CHECK(wake.watch(fence, 11u));
  CHECK_EQ(fence.descriptor, first);
  fence.reach();
  CHECK(woken_reaches(3u));
  wake.stop();
  wake.stop(); }

TEST(eventfd_wake_watch, an_ended_session_unbinds_and_its_fences_wake_nobody)
{ woken = 0u;
  EventfdWake wake;
  wake.bind(&::wake, &woken);
  ScriptedFence fence;
  CHECK(wake.watch(fence, 5u));
  const int first = fence.descriptor;
  wake.unbind();
  CHECK(fcntl(first, F_GETFD) != -1);
  fence.reach();
  fence.descriptor = -1;
  CHECK(wake.watch(fence, 6u));
  CHECK_EQ(fence.descriptor, -1);
  CHECK_EQ(woken.load(), 0u);

  std::atomic<std::uint32_t> next{0u};
  wake.bind(&::wake, &next);
  CHECK(wake.watch(fence, 7u));
  CHECK_EQ(fence.descriptor, first);
  const timespec settle = {0, 20000000};
  nanosleep(&settle, nullptr);
  CHECK_EQ(next.load(), 0u);
  fence.reach();
  for (std::uint32_t tries = 0u; tries < 5000u && next.load() == 0u; tries++) {
      const timespec pause = {0, 1000000};
      nanosleep(&pause, nullptr);
  }
  CHECK_EQ(next.load(), 1u);
  CHECK_EQ(woken.load(), 0u);
  wake.unbind(); }

TEST(eventfd_wake_watch, a_mark_vkd3d_proton_refuses_is_refused)
{ woken = 0u;
  EventfdWake wake;
  wake.bind(&::wake, &woken);
  ScriptedFence fence;
  fence.refuse = true;
  CHECK(!wake.watch(fence, 5u));
  fence.refuse = false;
  CHECK(wake.watch(fence, 6u));
  CHECK(fence.asked == 6u);
  CHECK_EQ(woken.load(), 0u); }

TEST(eventfd_wake, a_running_wake_stops_with_its_owner)
{ woken = 0u;
  ScriptedFence fence;
  int descriptor = -1;
  {
      EventfdWake wake;
      wake.bind(&::wake, &woken);
      CHECK(wake.watch(fence, 1u));
      descriptor = fence.descriptor;
  }
  CHECK(descriptor > 0);
  CHECK_EQ(fcntl(descriptor, F_GETFD), -1); }

TEST(eventfd_wake_watch, a_direct3d_11_fence_and_a_direct3d_12_one_wake_through_one_eventfd)
{ woken = 0u;
  EventfdWake wake;
  wake.bind(&::wake, &woken);
  ScriptedFence11 fence11;
  ScriptedFence   fence12;
  CHECK(wake.watch(fence11, 4u));
  CHECK(wake.watch(fence12, 7u));
  CHECK(fence11.asked == 4u);
  CHECK(fence11.descriptor > 0);
  CHECK_EQ(fence11.descriptor, fence12.descriptor);
  fence11.reach();
  CHECK(woken_reaches(1u));
  fence12.reach();
  CHECK(woken_reaches(2u));
  wake.unbind(); }
