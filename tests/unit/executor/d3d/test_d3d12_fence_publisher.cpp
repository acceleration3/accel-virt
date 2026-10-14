/* The Direct3D 12 fences whose completed values the guest reads from its own
 * memory: each published fence's value written, as one aligned u64, at the
 * offset the guest named, whenever it moves; a fence a queue will signal is
 * pending until it has. The fences are scripted, so the GPU moves when the
 * test says. */
#include "executor/d3d/d3d12_fence_publisher.hpp"
#include "executor/guest_region.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/executor/local_import.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::D3d12FencePublisher;
using av::GuestRegion;

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
    HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64, HANDLE) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Signal(UINT64 value) override
    {
        completed = value;
        return S_OK;
    }

    UINT64 completed = 0u;
};

/* A session's region over memory of the test's own, and the word at `offset`
 * in it. */
class Arena {
public:
    Arena() : bytes_{}, import_(bytes_, sizeof bytes_) { CHECK(region_.adopt(import_.blob())); }
    GuestRegion  &region() { return region_; }
    std::uint64_t word(std::uint64_t offset) const
    {
        std::uint64_t value = 0u;
        std::memcpy(&value, bytes_ + offset, sizeof value);
        return value;
    }

private:
    alignas(8) std::uint8_t bytes_[4096];
    av::LocalImport import_;
    GuestRegion     region_;
};

}  // namespace

TEST(d3d12_fence_publisher_publish, writes_the_value_at_once_and_again_whenever_it_moves)
{ Arena arena;
  D3d12FencePublisher publisher(arena.region());
  ScriptedFence fence;
  fence.completed = 4u;
  CHECK(publisher.publish(7u, &fence, 64u));
  CHECK(arena.word(64u) == 4u);
  CHECK(!publisher.update());
  fence.completed = 9u;
  CHECK(arena.word(64u) == 4u);   /* between batches, not as it moves */
  CHECK(!publisher.update());
  CHECK(arena.word(64u) == 9u);
  CHECK_EQ(publisher.count(), 1u);
  CHECK(publisher.invariants_hold()); }

TEST(d3d12_fence_publisher_publish, refuses_an_offset_outside_the_region_or_misaligned_and_no_fence)
{ Arena arena;
  D3d12FencePublisher publisher(arena.region());
  ScriptedFence fence;
  CHECK(!publisher.publish(1u, &fence, 4092u));   /* its eight bytes past the end */
  CHECK(!publisher.publish(1u, &fence, 4096u));
  CHECK(!publisher.publish(1u, &fence, 12u));
  CHECK(!publisher.publish(1u, nullptr, 16u));
  CHECK(publisher.publish(1u, &fence, 4088u));    /* the last word */
  CHECK_EQ(publisher.count(), 1u); }

/* Publishing a fence again moves it; offset `stop` ends it, as does forget. */
TEST(d3d12_fence_publisher_publish, a_fence_published_again_moves_and_stop_ends_it)
{ Arena arena;
  D3d12FencePublisher publisher(arena.region());
  ScriptedFence fence;
  fence.completed = 2u;
  CHECK(publisher.publish(3u, &fence, 8u));
  CHECK(publisher.publish(3u, &fence, 16u));
  CHECK_EQ(publisher.count(), 1u);
  fence.completed = 5u;
  CHECK(!publisher.update());
  CHECK(arena.word(8u) == 2u);
  CHECK(arena.word(16u) == 5u);
  CHECK(publisher.publish(3u, nullptr, D3d12FencePublisher::stop));
  CHECK_EQ(publisher.count(), 0u);
  fence.completed = 6u;
  CHECK(!publisher.update());
  CHECK(arena.word(16u) == 5u);
  CHECK(publisher.publish(9u, nullptr, D3d12FencePublisher::stop));   /* never published: nothing */ }

TEST(d3d12_fence_publisher_expect, a_fence_behind_what_a_queue_will_signal_is_pending)
{ Arena arena;
  D3d12FencePublisher publisher(arena.region());
  ScriptedFence first, second;
  CHECK(publisher.publish(1u, &first, 0u));
  CHECK(publisher.publish(2u, &second, 8u));
  publisher.expect(1u, 3u);
  publisher.expect(1u, 2u);   /* an older expectation never lowers it */
  publisher.expect(5u, 9u);   /* not published: nothing */
  CHECK(publisher.update());
  first.completed = 2u;
  CHECK(publisher.update());
  CHECK(arena.word(0u) == 2u);
  first.completed = 3u;
  CHECK(!publisher.update());
  CHECK(arena.word(0u) == 3u); }

TEST(d3d12_fence_publisher_forget, the_last_fence_takes_the_forgotten_one_s_place)
{ Arena arena;
  D3d12FencePublisher publisher(arena.region());
  ScriptedFence fences[3];
  for (std::uint32_t id = 0u; id < 3u; id++) CHECK(publisher.publish(id + 1u, &fences[id], id * 8u));
  publisher.expect(3u, 1u);
  publisher.forget(1u);
  publisher.forget(1u);
  CHECK_EQ(publisher.count(), 2u);
  CHECK(publisher.update());   /* the moved fence kept its expectation */
  fences[2].completed = 1u;
  fences[0].completed = 7u;
  CHECK(!publisher.update());
  CHECK(arena.word(16u) == 1u);
  CHECK(arena.word(0u) == 0u);   /* forgotten: no longer written */
  publisher.clear();
  CHECK_EQ(publisher.count(), 0u);
  CHECK(publisher.invariants_hold()); }

TEST(d3d12_fence_publisher_publish, a_full_table_refuses_the_next_fence)
{ alignas(8) static std::uint8_t bytes[D3d12FencePublisher::capacity * 8u + 8u];
  av::LocalImport import(bytes, sizeof bytes);
  GuestRegion     region;
  CHECK(region.adopt(import.blob()));
  D3d12FencePublisher publisher(region);
  ScriptedFence fence;
  for (std::uint32_t id = 1u; id <= D3d12FencePublisher::capacity; id++)
      CHECK(publisher.publish(id, &fence, (id - 1u) * 8u));
  CHECK(!publisher.publish(D3d12FencePublisher::capacity + 1u, &fence, D3d12FencePublisher::capacity * 8u));
  CHECK(publisher.publish(1u, &fence, D3d12FencePublisher::capacity * 8u));   /* one already here moves */
  CHECK_EQ(publisher.count(), D3d12FencePublisher::capacity); }
