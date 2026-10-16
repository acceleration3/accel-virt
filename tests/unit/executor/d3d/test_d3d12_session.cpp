/* The Direct3D 12 half of a Direct3D session with no device behind it - a host
 * whose GPU vkd3d-proton does not run - and a guest naming objects this
 * session never made. Every command must be refused, or answered as a device
 * that is not there, and nothing made: a guest's ids are foreign numbers, and
 * a handler that trusted one would hand vkd3d-proton a null object. What a
 * session does with a device is judged on the host GPU in
 * test_d3d12_session_device.cpp. */
#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d12_fence_publisher.hpp"
#include "executor/d3d/d3d12_session.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/guest_region.hpp"
#include "executor/imported_memories.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/executor/local_import.hpp"
#include "executor/wake_signal.hpp"
#include "executor/d3d/watched_blocks.hpp"
#include "executor/shim/host_heap.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::D3d12Session;
using av::GuestRegion;
using namespace av::d3d12proto;

namespace {

class Counted final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { return --references; }

    ULONG references = 1u;
};

constexpr std::uint32_t nobody = 77u;
constexpr std::uint32_t dxgi_error_unsupported = 0x887A0004u;

/* The session and the session's object table, a megabyte: in static storage,
 * made fresh for each test over a region of guest memory. */
av::ObjectStorage<D3d12Session> session_storage;
av::HostHeap                    session_heap;
av::WatchedBlocks               blocks(session_heap);
av::D3dObjectTable              objects(blocks);
av::WakeSignal tenant;
alignas(64) std::uint8_t guest[8192];

class FreshSession {
public:
    FreshSession() : import_(guest, sizeof guest)
    {
        CHECK(region_.adopt(import_.blob()));
        objects.clear();
        CHECK_STATUS(session_storage.construct(region_, imported_, objects, tenant, blocks), av::Status::ok);
        session_storage.object().initialize(nullptr);
    }
    ~FreshSession()
    {
        session_storage.object().shut_down();
        session_storage.destroy();
    }
    FreshSession(const FreshSession &) = delete;
    FreshSession &operator=(const FreshSession &) = delete;

    D3d12Session         &session() { return session_storage.object(); }
    av::ImportedMemories &imported() { return imported_; }

private:
    av::LocalImport      import_;
    GuestRegion          region_;
    av::ImportedMemories imported_;
};

}  // namespace

TEST(d3d12_session_guest_bytes_out, is_the_session_s_region_and_nothing_past_it)
{ FreshSession fresh;
  D3d12Session &session = fresh.session();
  CHECK(session.guest_bytes_out(0u, 16u) == guest);
  CHECK(session.guest_bytes_out(4096u, 4096u) == guest + 4096u);
  CHECK(session.guest_bytes_out(4096u, 4097u) == nullptr);
  CHECK(session.guest_bytes_out(sizeof guest, 1u) == nullptr);
  CHECK(session.guest_bytes(8u, 8u) == guest + 8u); }

TEST(d3d12_session_on_OpenDevice12, a_session_with_no_device_answers_unsupported_and_no_caps)
{ FreshSession fresh;
  std::uint32_t hresult = 0u;
  Caps12 caps = {};
  caps.feature_level = 0xC100u;
  CHECK(fresh.session().on_OpenDevice12(0u, hresult, caps));
  CHECK_EQ(hresult, dxgi_error_unsupported);
  CHECK_EQ(caps.feature_level, 0u); }

TEST(d3d12_session_on_QueryFormat12, a_session_with_no_device_supports_nothing)
{ FreshSession fresh;
  std::uint32_t support1 = 9u, support2 = 9u, levels = 9u;
  CHECK(!fresh.session().on_QueryFormat12(28u, 4u, support1, support2, levels));
  CHECK_EQ(support1, 0u);
  CHECK_EQ(support2, 0u);
  CHECK_EQ(levels, 0u); }

TEST(d3d12_session_on_ResourceAllocation12, a_session_with_no_device_answers_a_size_no_heap_holds)
{ FreshSession fresh;
  ResourceDesc desc = {};
  desc.width = 4096u;
  desc.dimension = 1u;
  std::uint64_t size = 0u, alignment = 7u;
  CHECK(fresh.session().on_ResourceAllocation12(desc, size, alignment));
  CHECK(size == ~std::uint64_t{0});
  CHECK(alignment == 0u); }

TEST(d3d12_session_on_Finish12, answers_the_removed_reason_and_nothing_else_without_a_device)
{ FreshSession fresh;
  std::uint32_t hresult = 5u;
  CHECK(fresh.session().on_Finish12(hresult));
  CHECK_EQ(hresult, 0u);
  CHECK_EQ(fresh.session().removed_reason(), 0u); }

TEST(d3d12_session, makes_nothing_without_a_device)
{ FreshSession fresh;
  D3d12Session &session = fresh.session();
  const HeapProperties upload = {2u, 0u, 0u, 0u};
  const HeapProperties device_local = {1u, 0u, 0u, 0u};
  ResourceDesc buffer = {};
  buffer.width = 256u;
  buffer.dimension = 1u;
  buffer.height = 1u;
  buffer.depth_or_array = 1u;
  buffer.mip_levels = 1u;
  buffer.sample_count = 1u;
  const ClearValue none = {};
  CHECK(!session.on_CreateHeap12(1u, upload, 0u, 4096u, 0u, 0u));
  CHECK(!session.on_CreateHeap12(1u, device_local, 0u, 4096u, 0u, 0u));
  /* Not even over memory the session imported for the heap. */
  alignas(4096) static std::uint8_t heap_memory[4096];
  av::LocalImport heap(heap_memory, sizeof heap_memory, 2u);
  CHECK(fresh.imported().adopt(heap.blob()));
  CHECK(!session.on_CreateHeap12(1u, upload, 0u, 4096u, 0u, heap.blob().wire_handle()));
  CHECK(objects.find(1u, av::D3dObjectKind::heap12) == nullptr);
  fresh.imported().abandon(heap.blob());
  CHECK(!session.on_CreateCommittedResource12(2u, device_local, 0u, buffer, 0u, none, 0u));
  CHECK(!session.on_CreatePlacedResource12(3u, 1u, 0u, buffer, 0u, none, 0u));
  CHECK(!session.on_CreateReservedResource12(3u, buffer, 0u, none, 0u));
  std::uint32_t tiles = 9u, packed_mips = 9u, packed_tiles = 9u;
  CHECK(!session.on_ResourceTiling12(3u, tiles, packed_mips, packed_tiles));
  CHECK_EQ(tiles, 0u);
  CHECK_EQ(packed_mips, 0u);
  CHECK_EQ(packed_tiles, 0u);
  CHECK(!session.on_CreateDescriptorHeap12(4u, 0u, 16u, 0u));
  CHECK(!session.on_CreateRootSignature12(5u, 0u, 0u, nullptr, 0u, nullptr, 0u, nullptr));
  CHECK(!session.on_CreateCommandQueue12(6u, 0u, 0u, 0u));
  CHECK(!session.on_CreateCommandAllocator12(7u, 0u));
  CHECK(!session.on_CreateCommandList12(8u, 7u, 0u, 0u));
  CHECK(!session.on_CreateFence12(9u, 0u, 0u));
  CHECK(!session.on_CreateQueryHeap12(10u, 0u, 4u));
  /* Pipelines are made in the decoding command's arena, which only a command
   * run through the session has. */
  const GraphicsState state = {};
  const std::uint8_t *const nothing = nullptr;
  CHECK(!session.on_CreateGraphicsPipeline12(11u, 5u, state, 0u, nullptr, 0u, nullptr, nothing, 0u, nothing, 0u,
                                            nothing, 0u, nothing, 0u, nothing, 0u));
  CHECK(!session.on_CreateComputePipeline12(12u, 5u, nothing, 0u));
  const IndirectArgument draw = {};
  CHECK(!session.on_CreateCommandSignature12(13u, 0u, 16u, 1u, &draw));
  /* None of those ids names anything now. */
  for (std::uint32_t id = 1u; id <= 13u; id++) CHECK(!session.on_Destroy12(id)); }

TEST(d3d12_session, every_view_and_descriptor_naming_a_heap_it_never_made_is_refused)
{ FreshSession fresh;
  D3d12Session &session = fresh.session();
  const DescriptorRef at = {nobody, 0u};
  const ViewDesc view = {};
  CHECK(!session.on_CreateShaderResourceView12(at, 0u, view));
  CHECK(!session.on_CreateUnorderedAccessView12(at, 1u, view));
  CHECK(!session.on_CreateRenderTargetView12(at, 0u, view));
  CHECK(!session.on_CreateDepthStencilView12(at, 1u, view));
  CHECK(!session.on_CreateConstantBufferView12(at, 0u, 256u));
  CHECK(!session.on_CreateSampler12(at, SamplerDesc{}));
  CHECK(!session.on_CopyDescriptors12(at, DescriptorRef{nobody + 1u, 0u}, 1u)); }

TEST(d3d12_session, every_queue_list_and_fence_command_naming_nothing_is_refused)
{ FreshSession fresh;
  D3d12Session &session = fresh.session();
  const std::uint32_t lists[1] = {nobody};
  std::uint64_t completed = 9u;
  CHECK(!session.on_ResetCommandAllocator12(nobody));
  CHECK(!session.on_ResetCommandList12(nobody, nobody, 0u));
  CHECK(!session.on_CloseCommandList12(nobody));
  CHECK(!session.on_ExecuteCommandLists12(nobody, 1u, lists));
  CHECK(!session.on_SignalFence12(0u, nobody, 1u));
  CHECK(!session.on_SignalFence12(nobody, nobody, 1u));
  CHECK(!session.on_WaitFence12(nobody, nobody, 1u));
  CHECK(!session.on_FenceValue12(nobody, 1u, 1u, completed));
  CHECK(completed == 0u);
  CHECK(!session.on_PublishFence12(nobody, 64u));
  /* Stopping a publication that never started is nothing to refuse. */
  CHECK(session.on_PublishFence12(nobody, av::D3d12FencePublisher::stop));
  /* A swap chain buffer shared or presented that nothing names. */
  CHECK(!session.on_ShareResource12(nobody, 0x5eedu));
  CHECK(!session.on_PresentResource12(nobody, nobody, 1u));
  /* A queue's tile mappings, on a queue and of resources nothing made. */
  const TileRegion region = {TileCoordinate{}, TileSize{1u, 0u, 0u, 0u, 0u}};
  const TileRange range = {0u, 0u, 1u};
  CHECK(!session.on_UpdateTileMappings12(nobody, nobody, 1u, &region, 0u, 1u, &range, 0u));
  CHECK(!session.on_UpdateTileMappings12(nobody, nobody, 1u, &region, nobody, 1u, &range, 0u));
  CHECK(!session.on_CopyTileMappings12(nobody, nobody, TileCoordinate{}, nobody, TileCoordinate{}, region.size, 0u)); }

TEST(d3d12_session, every_recording_command_on_a_list_it_never_made_is_refused)
{ FreshSession fresh;
  D3d12Session &session = fresh.session();
  const std::uint32_t list = nobody;
  const Barrier barrier = {};
  const std::uint32_t heaps[1] = {1u};
  const std::uint32_t constants[2] = {1u, 2u};
  const VertexBufferView vertices = {};
  const Viewport viewport = {};
  const Rect rect = {};
  const DescriptorRef ref = {1u, 0u};
  const Float4 color = {};
  const CopyLocation location = {};
  const Box box = {};
  const std::uint8_t marker[4] = {'m', 'a', 'r', 'k'};
  CHECK(!session.on_ResourceBarrier12(list, 1u, &barrier));
  CHECK(!session.on_SetPipelineState12(list, 1u));
  CHECK(!session.on_SetRootSignature12(list, 0u, 0u));
  CHECK(!session.on_SetDescriptorHeaps12(list, 1u, heaps));
  CHECK(!session.on_SetRootDescriptorTable12(list, 0u, 0u, ref));
  CHECK(!session.on_SetRootConstants12(list, 0u, 0u, 0u, 2u, constants));
  CHECK(!session.on_SetRootView12(list, 0u, 0u, 0u, 0x1000u));
  CHECK(!session.on_SetTopology12(list, 4u));
  CHECK(!session.on_SetVertexBuffers12(list, 0u, 1u, &vertices));
  CHECK(!session.on_SetIndexBuffer12(list, 0x1000u, 64u, 42u));
  CHECK(!session.on_SetViewports12(list, 1u, &viewport));
  CHECK(!session.on_SetScissorRects12(list, 1u, &rect));
  CHECK(!session.on_SetRenderTargets12(list, 1u, &ref, 0u, ref));
  CHECK(!session.on_SetBlendFactor12(list, color));
  CHECK(!session.on_SetStencilRef12(list, 1u));
  CHECK(!session.on_ClearRenderTargetView12(list, ref, color, 0u, nullptr));
  CHECK(!session.on_ClearDepthStencilView12(list, ref, 1u, 1.0f, 0u, 0u, nullptr));
  CHECK(!session.on_DrawInstanced12(list, 3u, 1u, 0u, 0u));
  CHECK(!session.on_DrawIndexedInstanced12(list, 3u, 1u, 0u, 0, 0u));
  CHECK(!session.on_Dispatch12(list, 1u, 1u, 1u));
  CHECK(!session.on_CopyBufferRegion12(list, 1u, 0u, 2u, 0u, 16u));
  CHECK(!session.on_CopyTextureRegion12(list, location, 0u, 0u, 0u, location, 0u, box));
  CHECK(!session.on_CopyResource12(list, 1u, 2u));
  CHECK(!session.on_ResolveSubresource12(list, 1u, 0u, 2u, 0u, 28u));
  CHECK(!session.on_BeginQuery12(list, 1u, 0u, 0u));
  CHECK(!session.on_EndQuery12(list, 1u, 0u, 0u));
  CHECK(!session.on_ResolveQueryData12(list, 1u, 0u, 0u, 1u, 2u, 0u));
  CHECK(!session.on_BeginEvent12(list, 1u, marker, sizeof marker));
  CHECK(!session.on_EndEvent12(list));
  CHECK(!session.on_SetMarker12(list, 1u, marker, sizeof marker));
  CHECK(!session.on_ExecuteIndirect12(list, 1u, 1u, 2u, 0u, 0u, 0u));
  CHECK(!session.on_CopyTiles12(list, 1u, TileCoordinate{}, TileSize{1u, 0u, 0u, 0u, 0u}, 2u, 0u, 0u)); }

/* With no queue executed there is no GPU work to wait for, and no published
 * fence to write. */
TEST(d3d12_session, a_session_that_executed_nothing_has_no_gpu_work_outstanding)
{ FreshSession fresh;
  std::uint64_t marked = 5u, reached = 5u;
  fresh.session().gpu_progress(marked, reached);
  CHECK(marked == 0u);
  CHECK(reached == 0u);
  CHECK(!fresh.session().poll()); }

/* The session's table is shared with Direct3D 11's and 9's: Destroy12 and the
 * session going touch only Direct3D 12's objects, and the invariant counts
 * only those against a session with no device. */
TEST(d3d12_session_objects, are_its_own_kinds_in_the_shared_table)
{ Counted eleven, twelve;
  { FreshSession fresh;
    D3d12Session &session = fresh.session();
    CHECK_STATUS(objects.insert(41u, av::D3dObjectKind::resource, &eleven), av::Status::ok);
    CHECK(session.invariants_hold());
    CHECK(!session.on_Destroy12(41u));
    CHECK_STATUS(objects.insert(42u, av::D3dObjectKind::query_heap12, &twelve), av::Status::ok);
    CHECK(!session.invariants_hold());
    CHECK(session.on_Destroy12(42u));
    CHECK_EQ(twelve.references, 0u);
    CHECK_STATUS(objects.insert(43u, av::D3dObjectKind::query9, &twelve), av::Status::ok);
    CHECK(!session.on_Destroy12(43u));
    objects.remove(43u); }
  /* The session went; the Direct3D 11 object is still the table's. */
  CHECK(objects.find(41u, av::D3dObjectKind::resource) == &eleven);
  objects.remove(41u);
  CHECK_EQ(eleven.references, 0u); }
