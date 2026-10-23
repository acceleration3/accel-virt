/* A Direct3D session's backend before the guest made a device on it, and a
 * guest naming objects it never made: every drawing, binding and copying
 * command is refused, every object command naming nothing is refused, and the
 * guest's memory is reached only inside the blob the session adopted. What a
 * backend does with a device is judged on the host GPU in
 * test_d3d_backend_device.cpp. */
#include "core/support/object_storage.hpp"
#include "device/abi/av_executor.h"
#include "executor/sharing/session_shares.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"
#include "executor/d3d/query_result_publisher.hpp"
#include "harness/executor/local_import.hpp"
#include "core/log/driver_log.hpp"
#include "executor/sharing/host_shares.hpp"
#include "harness/icd/recording_log.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/d3d_runtime.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

#include <vulkan/vulkan.h>

namespace av {

/* The session's object table, shared with Direct3D 12 and 9. */
struct D3dBackendProbe {
    static D3dObjectTable &objects(D3dBackend &backend) { return backend.objects_; }
};

}  // namespace av
using av::D3dBackend;
using av::ImportedBlob;
using av::LocalImport;
using namespace av::d3dproto;

namespace {

constexpr std::uint32_t nobody = 77u;
constexpr std::uint64_t page_bytes = 4096u;

/* Megabytes: in static storage, made fresh for each test. */
av::ObjectStorage<D3dBackend> backend_storage;
alignas(4096) std::uint8_t guest[2u * page_bytes];
alignas(4096) std::uint8_t store_bytes[page_bytes];

class FreshBackend {
public:
    FreshBackend() { CHECK_STATUS(backend_storage.construct(), av::Status::ok); }
    ~FreshBackend()
    {
        backend_storage.object().shut_down();
        backend_storage.destroy();
    }
    FreshBackend(const FreshBackend &) = delete;
    FreshBackend &operator=(const FreshBackend &) = delete;

    D3dBackend &backend() { return backend_storage.object(); }
};

std::uint64_t address_of(const av::SourceMemory &memory)
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&memory));
}

}  // namespace

/* The session's first import is its region, which bulk data and query
 * results are reached in: inside it, never past it. */
TEST(d3d_backend_guest_bytes_out, reaches_the_first_import_and_nothing_else)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  CHECK(backend.guest_bytes_out(0u, 1u) == nullptr);   /* nothing adopted yet */
  LocalImport region(guest, sizeof guest);
  CHECK(backend.adopt(region.blob()));
  CHECK(backend.guest_bytes_out(0u, 16u) == guest);
  CHECK(backend.guest_bytes_out(page_bytes, page_bytes) == guest + page_bytes);
  CHECK(backend.guest_bytes_out(page_bytes, page_bytes + 1u) == nullptr);
  CHECK(backend.guest_bytes(8u, 8u) == guest + 8u);
  backend.abandon(region.blob());
  CHECK(backend.guest_bytes_out(0u, 16u) == nullptr);
  CHECK(backend.invariants_hold()); }

/* Every import after the region is an object's own memory - a Direct3D 12
 * heap's, a slice store's - named by the memory's address, and never part of
 * the region: abandoning one leaves the region, and the region's going leaves
 * it. */
TEST(d3d_backend_adopt, imports_after_the_region_are_objects_memory_named_by_their_address)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  LocalImport region(guest, sizeof guest, 1u);
  LocalImport object(store_bytes, sizeof store_bytes, 2u);
  CHECK(backend.adopt(region.blob()));
  CHECK(backend.adopt(object.blob()));
  CHECK_EQ(object.blob().wire_handle(), address_of(object.memory()));
  CHECK_EQ(object.blob().object(), address_of(object.memory()));
  CHECK(backend.guest_bytes_out(0u, sizeof guest) == guest);   /* still the region */
  CHECK(backend.guest_bytes_out(sizeof guest, 1u) == nullptr);
  backend.abandon(object.blob());
  CHECK(backend.guest_bytes_out(0u, 16u) == guest);
  CHECK(backend.adopt(object.blob()));
  backend.abandon(region.blob());
  CHECK(backend.guest_bytes_out(0u, 16u) == nullptr);
  /* The next import is a region again. */
  LocalImport next(guest, page_bytes, 3u);
  CHECK(backend.adopt(next.blob()));
  CHECK(backend.guest_bytes_out(page_bytes - 1u, 1u) == guest + page_bytes - 1u);
  CHECK(backend.guest_bytes_out(page_bytes, 1u) == nullptr);
  backend.abandon(object.blob());
  backend.abandon(next.blob());
  CHECK(backend.invariants_hold()); }

/* An import the session never adopted made nothing, so abandoning it gives
 * nothing back - not even the region, whose memory it names. */
TEST(d3d_backend_abandon, an_import_never_adopted_gives_nothing_back)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  LocalImport region(guest, sizeof guest);
  CHECK(backend.adopt(region.blob()));
  ImportedBlob stranger;
  CHECK_STATUS(stranger.take(9u, region.memory()), av::Status::ok);
  backend.abandon(stranger);
  CHECK(backend.guest_bytes_out(0u, 16u) == guest);
  static_cast<void>(stranger.give_back());
  backend.abandon(region.blob());
  CHECK(backend.invariants_hold()); }

/* Query results go to slots in the region: all of them inside it, and a
 * sensible number of them. */
TEST(d3d_backend_on_BindQueryResults, binds_slots_only_inside_the_arena)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  CHECK(!backend.on_BindQueryResults(0u, 1u));   /* no arena */
  LocalImport region(guest, sizeof guest);
  CHECK(backend.adopt(region.blob()));
  const std::uint32_t fit = sizeof guest / av::QueryResultPublisher::slot_bytes;
  CHECK(backend.on_BindQueryResults(0u, fit));
  CHECK(!backend.on_BindQueryResults(0u, fit + 1u));
  CHECK(!backend.on_BindQueryResults(av::QueryResultPublisher::slot_bytes, fit));
  CHECK(!backend.on_BindQueryResults(0u, 0u));
  CHECK(!backend.on_BindQueryResults(0u, 4097u));
  backend.abandon(region.blob()); }

/* A session on no GPU draws nothing in place: it answers that buffers have no
 * slices, and binds none - not even to a store it imported. */
TEST(d3d_backend_on_BindBufferSlice, a_session_with_no_device_has_no_slices)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  std::uint32_t supported = 1u;
  CHECK(backend.on_QueryBufferSlices11(supported));
  CHECK_EQ(supported, 0u);
  LocalImport region(guest, sizeof guest, 1u);
  LocalImport store(store_bytes, sizeof store_bytes, 2u);
  CHECK(backend.adopt(region.blob()));
  CHECK(backend.adopt(store.blob()));
  CHECK(!backend.on_BindBufferSlice(1u, store.blob().wire_handle(), 0u, 256u));
  backend.abandon(store.blob());
  backend.abandon(region.blob()); }

TEST(d3d_backend, makes_nothing_before_the_guest_made_a_device)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  ResourceDesc resource = {};
  resource.width = 256u;
  const std::uint8_t code[8] = {};
  const InputElement element = {};
  std::uint32_t found = 9u;
  ResourceDesc opened = {};
  opened.width = 5u;
  std::uint32_t hresult = 9u;
  CHECK(!backend.on_CreateResource(1u, resource, 0u, 0u, 0u));
  CHECK(!backend.on_OpenResource(2u, 0x51u, found, opened));
  CHECK_EQ(found, 0u);
  CHECK(opened.width == 0u);
  CHECK(!backend.on_CreateShader(3u, 0u, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0u, code,
                                 sizeof code));
  CHECK(!backend.on_CreateInputLayout(4u, 1u, &element));
  CHECK(!backend.on_CreateBlendState(5u, BlendDesc{}));
  CHECK(!backend.on_CreateDepthStencilState(6u, DepthStencilDesc{}));
  CHECK(!backend.on_CreateRasterizerState(7u, RasterizerDesc{}));
  CHECK(!backend.on_CreateSampler(8u, SamplerDesc{}));
  CHECK(!backend.on_CreateQuery(9u, 0u, 0u));
  CHECK(!backend.on_Flush11());
  CHECK(!backend.on_ClearState11());
  CHECK(!backend.on_Finish11(hresult));
  const std::uint32_t pair[2] = {1u, 2u};
  CHECK(!backend.on_RotateResources11(2u, pair));
  CHECK(!backend.on_DestroyDevice11(nobody));
  for (std::uint32_t id = 1u; id <= 9u; id++) CHECK(!backend.on_DestroyObject(id));
  CHECK(backend.invariants_hold()); }

TEST(d3d_backend, a_view_of_a_resource_it_never_made_is_refused)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  ViewDesc desc = {};
  desc.resource = nobody;
  CHECK(!backend.on_CreateShaderResourceView(1u, desc));
  CHECK(!backend.on_CreateRenderTargetView(2u, desc));
  CHECK(!backend.on_CreateDepthStencilView(3u, desc));
  CHECK(!backend.on_CreateUnorderedAccessView(4u, desc)); }

/* With no device selected nothing is bound, drawn, cleared or copied, and an
 * end the guest sent is still counted - the guest counted it when it sent it. */
TEST(d3d_backend, nothing_is_bound_drawn_cleared_or_copied_with_no_device_selected)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  const std::uint32_t ids[1] = {nobody};
  const ConstantBufferBinding constants = {};
  const UavBinding uav = {};
  const VertexBufferBinding vertices = {};
  const StreamOutTarget stream = {};
  const Viewport viewport = {};
  const Rect rect = {};
  const Float4 color = {};
  const Uint4 values = {};
  const Box box = {};
  const std::uint8_t data[16] = {};
  std::uint8_t   out[16] = {};
  std::uint32_t  hresult = 0u, row_pitch = 0u, depth_pitch = 0u, ready = 9u;
  std::uint64_t  size = 9u;
  CHECK(!backend.on_SetShader(0u, nobody));
  CHECK(!backend.on_SetConstantBuffers(0u, 0u, 1u, &constants));
  CHECK(!backend.on_SetShaderResources(0u, 0u, 1u, ids));
  CHECK(!backend.on_SetSamplers(0u, 0u, 1u, ids));
  CHECK(!backend.on_SetComputeUnorderedAccessViews(0u, 1u, &uav));
  CHECK(!backend.on_SetRenderTargets(1u, ids, 0u, 0u, 0u, nullptr));
  CHECK(!backend.on_SetInputLayout(0u));
  CHECK(!backend.on_SetVertexBuffers(0u, 1u, &vertices));
  CHECK(!backend.on_SetIndexBuffer(0u, 42u, 0u));
  CHECK(!backend.on_SetTopology(4u));
  CHECK(!backend.on_SetViewports(1u, &viewport));
  CHECK(!backend.on_SetScissorRects(1u, &rect));
  CHECK(!backend.on_SetBlendState(0u, color, 0xFFFFFFFFu));
  CHECK(!backend.on_SetDepthStencilState(0u, 0u));
  CHECK(!backend.on_SetRasterizerState(0u));
  CHECK(!backend.on_SetStreamOutTargets(1u, &stream));
  CHECK(!backend.on_SetPredication(0u, 0u));
  CHECK(!backend.on_SetResourceMinLod(nobody, 1.0f));
  CHECK(!backend.on_Draw(3u, 0u));
  CHECK(!backend.on_DrawIndexed(3u, 0u, 0));
  CHECK(!backend.on_DrawInstanced(3u, 1u, 0u, 0u));
  CHECK(!backend.on_DrawIndexedInstanced(3u, 1u, 0u, 0, 0u));
  CHECK(!backend.on_DrawAuto());
  CHECK(!backend.on_DrawInstancedIndirect(nobody, 0u));
  CHECK(!backend.on_DrawIndexedInstancedIndirect(nobody, 0u));
  CHECK(!backend.on_Dispatch(1u, 1u, 1u));
  CHECK(!backend.on_DispatchIndirect(nobody, 0u));
  CHECK(!backend.on_ClearRenderTargetView(nobody, color));
  CHECK(!backend.on_ClearDepthStencilView(nobody, 1u, 1.0f, 0u));
  CHECK(!backend.on_ClearUnorderedAccessViewUint(nobody, values));
  CHECK(!backend.on_ClearUnorderedAccessViewFloat(nobody, color));
  CHECK(!backend.on_ClearView(nobody, color, 1u, &rect));
  CHECK(!backend.on_Discard(nobody, 0u, nullptr));
  CHECK(!backend.on_CopyResource(1u, 2u));
  CHECK(!backend.on_CopySubresourceRegion(1u, 0u, 0u, 0u, 0u, 2u, 0u, 0u, box, 0u));
  CHECK(!backend.on_ResolveSubresource(1u, 0u, 2u, 0u, 28u));
  CHECK(!backend.on_GenerateMips(nobody));
  CHECK(!backend.on_CopyStructureCount(1u, 0u, 2u));
  CHECK(!backend.on_UpdateSubresource(nobody, 0u, 0u, box, 16u, 16u, 0u, data, sizeof data));
  CHECK(!backend.on_ReadSubresource(nobody, 0u, hresult, row_pitch, depth_pitch, out, sizeof out, size));
  CHECK(!backend.on_PresentPoint(nobody, 0u, 1u));
  CHECK(!backend.on_BeginQuery(nobody));
  CHECK(!backend.on_EndQuery(nobody));
  CHECK(!backend.on_GetQueryData(nobody, 0u, ready, out, sizeof out, size));
  CHECK_EQ(ready, 0u);
  CHECK(size == 0u);
  CHECK(backend.invariants_hold()); }

/* No Direct3D 12 queue executed and no query pending: no GPU work outstanding,
 * nothing to publish, and a batch's end holds nothing to let go. */
TEST(d3d_backend, a_backend_with_no_work_has_nothing_outstanding)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  std::uint64_t marked = 3u, reached = 3u;
  backend.gpu_progress(marked, reached);
  CHECK(marked == 0u);
  CHECK(reached == 0u);
  CHECK(!backend.poll());
  backend.end_batch();
  CHECK(!backend.lost()); }

/* ---- on the host GPU ------------------------------------------------------ */

namespace {

bool first_gpu(std::uint8_t *uuid)
{
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.apiVersion = (1u << 22u) | (1u << 12u);   /* 1.1 */
    VkInstanceCreateInfo made = {};
    made.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    made.pApplicationInfo = &application;
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&made, nullptr, &instance) != VK_SUCCESS) return false;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    std::uint32_t    count = 1u;
    const bool       found = vkEnumeratePhysicalDevices(instance, &count, &physical) >= 0 && count != 0u;
    if (found) {
        VkPhysicalDeviceIDProperties ids = {};
        ids.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        VkPhysicalDeviceProperties2 properties = {};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties.pNext = &ids;
        vkGetPhysicalDeviceProperties2(physical, &properties);
        std::memcpy(uuid, ids.deviceUUID, VK_UUID_SIZE);
    }
    vkDestroyInstance(instance, nullptr);
    return found;
}

/* What the child saw of a selection. */
struct Selected {
    std::uint32_t ran;
    std::uint32_t created_hresult;
    std::uint32_t selected_known;
    std::uint32_t drew_on_known;
    std::uint32_t selected_unknown;
    std::uint32_t drew_on_unknown;
    std::uint32_t selected_again;
    std::uint32_t drew_again;
};

/* The guest's devices are context states of one host device, swapped by
 * SelectDevice11 under the device's lock, as run() holds it. */
Selected select_devices()
{
    Selected seen = {};
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::D3dRuntime      runtime;
    av::HostHeap        heap;
    av::RecordingLog    log;
    av::DriverLog       lines(log, "shares: ");
    av::HostShares      shares(heap, lines);
    av::D3d11HostDevice device11;
    std::uint8_t        uuid[VK_UUID_SIZE] = {};
    if (!runtime.open() || !first_gpu(uuid) || !device11.open(runtime, uuid, shares)) return seen;
    FreshBackend fresh;
    D3dBackend  &backend = fresh.backend();
    av::SessionShares unshared;
    if (!backend.initialize(unshared, &device11, nullptr)) return seen;
    seen.ran = 1u;
    device11.lock(nullptr);
    std::uint32_t hresult = 1u, level = 0u;
    static_cast<void>(backend.on_CreateDevice11(1u, 0u, 0u, hresult, level));
    seen.created_hresult = hresult;
    seen.selected_known = backend.on_SelectDevice11(1u) ? 1u : 0u;
    seen.drew_on_known = backend.on_Draw(3u, 0u) ? 1u : 0u;
    seen.selected_unknown = backend.on_SelectDevice11(99u) ? 1u : 0u;
    seen.drew_on_unknown = backend.on_Draw(3u, 0u) ? 1u : 0u;
    seen.selected_again = backend.on_SelectDevice11(1u) ? 1u : 0u;
    seen.drew_again = backend.on_Draw(3u, 0u) ? 1u : 0u;
    device11.unlock();
    backend.shut_down();
    return seen;
}

}  // namespace

/* A selection of a device the host made takes its state, and draws run; one
 * of an id with no device is accepted - refusing it would remove whichever
 * device spoke before - and what follows until the next selection is dropped.
 * DXVK's threads leak from modules it unloads, so the GPU's part runs in a
 * child that leaves with _exit. */
TEST(d3d_backend_on_SelectDevice11, a_known_device_is_drawn_on_and_an_unknown_one_drops_what_follows)
{ int channel[2] = {-1, -1};
  CHECK_EQ(pipe(channel), 0);
  const pid_t child = fork();
  CHECK(child >= 0);
  if (child == 0) {
      const Selected seen = select_devices();
      static_cast<void>(write(channel[1], &seen, sizeof seen));
      _exit(0);
  }
  close(channel[1]);
  Selected seen = {};
  const ssize_t got = read(channel[0], &seen, sizeof seen);
  close(channel[0]);
  int status = 0;
  CHECK_EQ(waitpid(child, &status, 0), child);
  CHECK_EQ(got, static_cast<ssize_t>(sizeof seen));
  if (seen.ran == 0u) return;   /* no DXVK on a GPU here: nothing to judge */
  CHECK_EQ(seen.created_hresult, 0u);
  CHECK_EQ(seen.selected_known, 1u);
  CHECK_EQ(seen.drew_on_known, 1u);
  CHECK_EQ(seen.selected_unknown, 1u);
  CHECK_EQ(seen.drew_on_unknown, 0u);
  CHECK_EQ(seen.selected_again, 1u);
  CHECK_EQ(seen.drew_again, 1u); }

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

}  // namespace

/* DestroyObject removes Direct3D 11's objects; Direct3D 12's and 9's in the
 * same table are theirs to destroy. */
TEST(d3d_backend_on_DestroyObject, removes_only_direct3d_11_objects)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  av::D3dObjectTable &objects = av::D3dBackendProbe::objects(backend);
  Counted eleven, twelve, nine;
  CHECK_STATUS(objects.insert(51u, av::D3dObjectKind::resource, &eleven), av::Status::ok);
  CHECK_STATUS(objects.insert(52u, av::D3dObjectKind::heap12, &twelve), av::Status::ok);
  CHECK_STATUS(objects.insert(53u, av::D3dObjectKind::surface9, &nine), av::Status::ok);
  CHECK(!backend.on_DestroyObject(52u));
  CHECK(!backend.on_DestroyObject(53u));
  CHECK_EQ(twelve.references, 1u);
  CHECK(backend.on_DestroyObject(51u));
  CHECK_EQ(eleven.references, 0u);
  objects.remove(52u);
  objects.remove(53u); }
