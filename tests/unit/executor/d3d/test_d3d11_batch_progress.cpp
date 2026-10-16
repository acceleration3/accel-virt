/* How far the host GPU has got with a Direct3D 11 session's batches: each
 * batch's end signals the session's fence with the next mark, reached() is
 * what the fence completed - never past the last mark - and a released fence
 * leaves every mark reached. Each mark is watched for by the session's wake,
 * and only a mark the runtime refused to watch is polled for. The fence is a
 * scripted stand-in for DXVK's, so the test says when the GPU got where. */
#include "executor/d3d/d3d11_batch_progress.hpp"
#include "executor/eventfd_wake.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d11BatchProgress;
using av::EventfdWake;

namespace {

class ScriptedFence final : public ID3D11Fence {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE   AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE   Release() override { return --references; }
    void STDMETHODCALLTYPE    GetDevice(ID3D11Device **device) override { *device = nullptr; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateSharedHandle(const SECURITY_ATTRIBUTES *, DWORD, LPCWSTR, HANDLE *) override
    {
        return E_NOTIMPL;
    }
    UINT64 STDMETHODCALLTYPE  GetCompletedValue() override { return completed; }
    HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64 value, HANDLE event) override
    {
        asked = value;
        handed = event;
        return answer;
    }

    UINT64  completed = 0u;
    UINT64  asked = 0u;
    HANDLE  handed = nullptr;
    HRESULT answer = E_NOTIMPL;
    ULONG  references = 1u;
};

}  // namespace

TEST(d3d11_batch_progress_new, has_marked_nothing_and_waits_for_nothing)
{ EventfdWake        wake;
  D3d11BatchProgress progress(wake);
  CHECK(progress.fence() == nullptr);
  CHECK(progress.marked() == 0u);
  CHECK(progress.next() == 1u);
  CHECK(progress.reached() == 0u);
  CHECK(!progress.pending());
  CHECK(progress.invariants_hold()); }

TEST(d3d11_batch_progress_signalled, marks_nothing_without_a_fence)
{ EventfdWake        wake;
  D3d11BatchProgress progress(wake);
  progress.adopt(nullptr);
  CHECK(progress.fence() == nullptr);
  progress.signalled();
  CHECK(progress.marked() == 0u);
  CHECK(progress.next() == 1u);
  CHECK(!progress.pending());
  CHECK(progress.invariants_hold()); }

TEST(d3d11_batch_progress_reached, follows_the_fence_and_is_pending_until_it_passes_the_last_mark)
{ ScriptedFence fence;
  { EventfdWake        wake;
    D3d11BatchProgress progress(wake);
    progress.adopt(&fence);
    CHECK(progress.fence() == &fence);
    progress.signalled();
    progress.signalled();
    CHECK(progress.marked() == 2u);
    CHECK(progress.next() == 3u);
    CHECK(progress.reached() == 0u);
    CHECK(progress.pending());
    fence.completed = 1u;
    CHECK(progress.reached() == 1u);
    CHECK(progress.pending());
    fence.completed = 2u;
    CHECK(progress.reached() == 2u);
    CHECK(!progress.pending());
    CHECK(progress.invariants_hold()); }
  CHECK_EQ(fence.references, 0u); }   /* the session's fence goes with it */

TEST(d3d11_batch_progress_reached, never_passes_the_last_mark_whatever_the_fence_answers)
{ ScriptedFence fence;
  EventfdWake        wake;
  D3d11BatchProgress progress(wake);
  progress.adopt(&fence);
  progress.signalled();
  fence.completed = UINT64_MAX;   /* a lost device's answer */
  CHECK(progress.reached() == 1u);
  CHECK(!progress.pending()); }

TEST(d3d11_batch_progress_adopt, keeps_the_fence_it_holds_and_lets_another_go)
{ ScriptedFence held;
  ScriptedFence other;
  EventfdWake        wake;
  D3d11BatchProgress progress(wake);
  progress.adopt(&held);
  progress.adopt(&other);
  CHECK(progress.fence() == &held);
  CHECK_EQ(other.references, 0u);
  CHECK_EQ(held.references, 1u); }

TEST(d3d11_batch_progress_release, leaves_every_mark_reached_and_marks_keep_rising)
{ ScriptedFence first;
  ScriptedFence second;
  EventfdWake        wake;
  D3d11BatchProgress progress(wake);
  progress.adopt(&first);
  progress.signalled();
  progress.signalled();
  progress.release();
  CHECK_EQ(first.references, 0u);
  CHECK(progress.fence() == nullptr);
  CHECK(progress.reached() == 2u);
  CHECK(!progress.pending());
  CHECK(progress.invariants_hold());
  progress.release();   /* nothing held: nothing let go twice */
  CHECK_EQ(first.references, 0u);
  progress.adopt(&second);
  second.completed = 2u;   /* made at marked(), as the device makes it */
  CHECK(progress.next() == 3u);
  progress.signalled();
  CHECK(progress.marked() == 3u);
  CHECK(progress.pending());
  second.completed = 3u;
  CHECK(!progress.pending());
  CHECK(progress.invariants_hold()); }

namespace {

void count_wake(void *count)
{
    ++*static_cast<std::uint32_t *>(count);
}

}  // namespace

/* With a worker bound, each mark is handed to DXVK with the wake's eventfd:
 * the worker sleeps until the GPU reaches it and polls for nothing. */
TEST(d3d11_batch_progress_signalled, a_watched_mark_is_not_polled_for)
{ std::uint32_t      woken = 0u;
  ScriptedFence      fence;
  fence.answer = S_OK;
  EventfdWake        wake;
  wake.bind(&count_wake, &woken);
  D3d11BatchProgress progress(wake);
  progress.adopt(&fence);
  progress.signalled();
  CHECK(fence.asked == 1u);
  CHECK(fence.handed != nullptr);
  CHECK(progress.pending());
  CHECK(!progress.polled());
  progress.signalled();
  CHECK(fence.asked == 2u);
  CHECK(!progress.polled());
  CHECK(progress.invariants_hold());
  progress.release();
  wake.unbind(); }

/* A mark DXVK will not watch is polled for until the GPU reaches it, and a
 * later one it does watch is not. */
TEST(d3d11_batch_progress_signalled, a_mark_the_runtime_refuses_to_watch_is_polled_for_until_reached)
{ std::uint32_t      woken = 0u;
  ScriptedFence      fence;
  EventfdWake        wake;
  wake.bind(&count_wake, &woken);
  D3d11BatchProgress progress(wake);
  progress.adopt(&fence);
  progress.signalled();
  CHECK(fence.asked == 1u);
  CHECK(progress.polled());
  fence.completed = 1u;
  CHECK(!progress.polled());
  fence.answer = S_OK;
  progress.signalled();
  CHECK(progress.pending());
  CHECK(!progress.polled());
  progress.release();
  CHECK(!progress.polled());
  CHECK(progress.invariants_hold());
  wake.unbind(); }
