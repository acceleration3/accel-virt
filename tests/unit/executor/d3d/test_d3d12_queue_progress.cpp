#include "core/log/driver_log.hpp"
#include "executor/d3d/d3d12_fence_wake.hpp"
#include "executor/d3d/d3d12_queue_progress.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <cstring>

using av::D3d12FenceWake;
using av::D3d12QueueProgress;

namespace {

class ScriptedFence final : public ID3D12Fence {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(const WCHAR *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID, void **device) override
    {
        *device = nullptr;
        return E_NOTIMPL;
    }
    ULONG STDMETHODCALLTYPE   AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE   Release() override { return --references; }
    UINT64 STDMETHODCALLTYPE  GetCompletedValue() override { return completed; }
    HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64, HANDLE) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Signal(UINT64 value) override
    {
        completed = value;
        return S_OK;
    }

    UINT64 completed = 0u;
    ULONG  references = 1u;
};

class ScriptedQueue final : public ID3D12CommandQueue {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(const WCHAR *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID, void **device) override
    {
        *device = nullptr;
        return E_NOTIMPL;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1u; }
    ULONG STDMETHODCALLTYPE Release() override { return 1u; }
    void STDMETHODCALLTYPE UpdateTileMappings(ID3D12Resource *, UINT, const D3D12_TILED_RESOURCE_COORDINATE *, const D3D12_TILE_REGION_SIZE *, UINT, const D3D12_TILE_RANGE_FLAGS *, UINT *, UINT *, D3D12_TILE_MAPPING_FLAGS) override {}
    void STDMETHODCALLTYPE CopyTileMappings(ID3D12Resource *, const D3D12_TILED_RESOURCE_COORDINATE *, ID3D12Resource *, const D3D12_TILED_RESOURCE_COORDINATE *, const D3D12_TILE_REGION_SIZE *, D3D12_TILE_MAPPING_FLAGS) override {}
    void STDMETHODCALLTYPE ExecuteCommandLists(UINT, ID3D12CommandList *const *) override {}
    void STDMETHODCALLTYPE SetMarker(UINT, const void *, UINT) override {}
    void STDMETHODCALLTYPE BeginEvent(UINT, const void *, UINT) override {}
    void STDMETHODCALLTYPE EndEvent() override {}
    HRESULT STDMETHODCALLTYPE GetTimestampFrequency(UINT64 *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetClockCalibration(UINT64 *, UINT64 *) override { return E_NOTIMPL; }
    D3D12_COMMAND_QUEUE_DESC* STDMETHODCALLTYPE GetDesc(D3D12_COMMAND_QUEUE_DESC * answer) override { return answer; }
    HRESULT STDMETHODCALLTYPE Signal(ID3D12Fence *fence, UINT64 value) override
    {
        if (refuse_signal) return E_FAIL;
        signalled = fence;
        signalled_value = value;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Wait(ID3D12Fence *fence, UINT64 value) override
    {
        if (refuse_wait) return E_FAIL;
        if (waits < 4u) {
            waited[waits] = fence;
            waited_value[waits] = value;
        }
        waits++;
        return S_OK;
    }

    ID3D12Fence  *signalled = nullptr;
    UINT64        signalled_value = 0u;
    ID3D12Fence  *waited[4] = {};
    UINT64        waited_value[4] = {};
    std::uint32_t waits = 0u;
    bool          refuse_signal = false;
    bool          refuse_wait = false;
};

class ScriptedDevice final : public ID3D12Device {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(const WCHAR *) override { return E_NOTIMPL; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1u; }
    ULONG STDMETHODCALLTYPE Release() override { return 1u; }
    UINT STDMETHODCALLTYPE GetNodeCount() override { return 0u; }
    HRESULT STDMETHODCALLTYPE CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateCommandList(UINT, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator *, ID3D12PipelineState *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(D3D12_FEATURE, void *, UINT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    UINT STDMETHODCALLTYPE GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE) override { return 0u; }
    HRESULT STDMETHODCALLTYPE CreateRootSignature(UINT, const void *, SIZE_T, REFIID, void **) override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override {}
    void STDMETHODCALLTYPE CreateShaderResourceView(ID3D12Resource *, const D3D12_SHADER_RESOURCE_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override {}
    void STDMETHODCALLTYPE CreateUnorderedAccessView(ID3D12Resource *, ID3D12Resource *, const D3D12_UNORDERED_ACCESS_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override {}
    void STDMETHODCALLTYPE CreateRenderTargetView(ID3D12Resource *, const D3D12_RENDER_TARGET_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override {}
    void STDMETHODCALLTYPE CreateDepthStencilView(ID3D12Resource *, const D3D12_DEPTH_STENCIL_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override {}
    void STDMETHODCALLTYPE CreateSampler(const D3D12_SAMPLER_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override {}
    void STDMETHODCALLTYPE CopyDescriptors(UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, const UINT *, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, const UINT *, D3D12_DESCRIPTOR_HEAP_TYPE) override {}
    void STDMETHODCALLTYPE CopyDescriptorsSimple(UINT, const D3D12_CPU_DESCRIPTOR_HANDLE, const D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_DESCRIPTOR_HEAP_TYPE) override {}
    D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE GetResourceAllocationInfo(D3D12_RESOURCE_ALLOCATION_INFO * answer, UINT, UINT, const D3D12_RESOURCE_DESC *) override { return answer; }
    D3D12_HEAP_PROPERTIES* STDMETHODCALLTYPE GetCustomHeapProperties(D3D12_HEAP_PROPERTIES * answer, UINT, D3D12_HEAP_TYPE) override { return answer; }
    HRESULT STDMETHODCALLTYPE CreateCommittedResource(const D3D12_HEAP_PROPERTIES *, D3D12_HEAP_FLAGS, const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateHeap(const D3D12_HEAP_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreatePlacedResource(ID3D12Heap *, UINT64, const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateReservedResource(const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateSharedHandle(ID3D12DeviceChild *, const SECURITY_ATTRIBUTES *, DWORD, const WCHAR *, HANDLE *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OpenSharedHandle(HANDLE, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OpenSharedHandleByName(const WCHAR *, DWORD, HANDLE *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE MakeResident(UINT, ID3D12Pageable *const *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Evict(UINT, ID3D12Pageable *const *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE GetCopyableFootprints(const D3D12_RESOURCE_DESC *, UINT, UINT, UINT64, D3D12_PLACED_SUBRESOURCE_FOOTPRINT *, UINT *, UINT64 *, UINT64 *) override {}
    HRESULT STDMETHODCALLTYPE CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetStablePowerState(WINBOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *, ID3D12RootSignature *, REFIID, void **) override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE GetResourceTiling(ID3D12Resource *, UINT *, D3D12_PACKED_MIP_INFO *, D3D12_TILE_SHAPE *, UINT *, UINT, D3D12_SUBRESOURCE_TILING *) override {}
    LUID* STDMETHODCALLTYPE GetAdapterLuid(LUID * answer) override { return answer; }
    HRESULT STDMETHODCALLTYPE CreateFence(UINT64 initial, D3D12_FENCE_FLAGS, REFIID, void **fence) override
    {
        if (refuse_fence || made == 64u) return E_OUTOFMEMORY;
        fences[made].completed = initial;
        *fence = &fences[made++];
        return S_OK;
    }

    ScriptedFence fences[64];
    std::uint32_t made = 0u;
    bool          refuse_fence = false;
};

}

TEST(d3d12_queue_progress_executed, each_execute_signals_its_queue_s_fence_with_the_next_mark)
{ av::MockPlatform   platform;
  D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake, platform);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy;
  CHECK(progress.marked() == 0u);
  CHECK(progress.reached() == 0u);
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(direct.signalled == &device.fences[0]);
  CHECK(direct.signalled_value == 1u);
  CHECK(progress.executed(2u, &copy, &device));
  CHECK(copy.signalled == &device.fences[1]);
  CHECK(copy.signalled_value == 2u);
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(direct.signalled == &device.fences[0]);
  CHECK(direct.signalled_value == 3u);
  CHECK_EQ(device.made, 2u);
  CHECK(progress.marked() == 3u);
  CHECK(progress.invariants_hold()); }

TEST(d3d12_queue_progress_reached, is_held_by_the_queue_furthest_behind_its_last_signal)
{ av::MockPlatform   platform;
  D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake, platform);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy;
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(progress.executed(2u, &copy, &device));
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(progress.reached() == 0u);
  device.fences[1].completed = 2u;
  CHECK(progress.reached() == 0u);
  device.fences[0].completed = 1u;
  CHECK(progress.reached() == 1u);
  device.fences[0].completed = 3u;
  CHECK(progress.reached() == 3u);
  CHECK(progress.reached() == progress.marked()); }

TEST(d3d12_queue_progress_after_siblings, waits_on_every_other_queue_still_behind_and_never_its_own)
{ av::MockPlatform   platform;
  D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake, platform);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy, compute;
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(progress.executed(2u, &copy, &device));
  CHECK(progress.executed(3u, &compute, &device));
  device.fences[2].completed = 3u;
  CHECK(progress.after_siblings(1u, &direct));
  CHECK_EQ(direct.waits, 1u);
  CHECK(direct.waited[0] == &device.fences[1]);
  CHECK(direct.waited_value[0] == 2u);
  device.fences[1].completed = 2u;
  CHECK(progress.after_siblings(1u, &direct));
  CHECK_EQ(direct.waits, 1u);
  CHECK(progress.after_siblings(9u, &direct));
  CHECK_EQ(direct.waits, 2u);
  CHECK(direct.waited[1] == &device.fences[0]);
  device.fences[0].completed = 1u;
  CHECK(progress.after_siblings(9u, &direct));
  CHECK_EQ(direct.waits, 2u);
  device.fences[0].completed = 0u;
  CHECK(progress.after_siblings(2u, &copy));
  CHECK_EQ(copy.waits, 1u);
  CHECK(copy.waited[0] == &device.fences[0]);
  CHECK(copy.waited_value[0] == 1u); }

TEST(d3d12_queue_progress_after_siblings, a_refused_wait_or_no_queue_is_refused)
{ av::MockPlatform   platform;
  D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake, platform);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy;
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(!progress.after_siblings(2u, nullptr));
  copy.refuse_wait = true;
  CHECK(!progress.after_siblings(2u, &copy));
  CHECK(progress.after_siblings(1u, &copy));   }

TEST(d3d12_queue_progress_executed, refuses_what_cannot_be_made_or_signalled)
{ av::MockPlatform   platform;
  D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake, platform);
  ScriptedDevice     device;
  ScriptedQueue      queue;
  CHECK(!progress.executed(1u, nullptr, &device));
  CHECK(!progress.executed(1u, &queue, nullptr));
  device.refuse_fence = true;
  CHECK(!progress.executed(1u, &queue, &device));
  device.refuse_fence = false;
  queue.refuse_signal = true;
  CHECK(!progress.executed(1u, &queue, &device));
  CHECK(progress.marked() == 0u);
  queue.refuse_signal = false;
  CHECK(progress.executed(1u, &queue, &device));
  CHECK(progress.marked() == 1u);
  CHECK(progress.invariants_hold()); }

TEST(d3d12_queue_progress_executed, serves_well_more_queues_than_the_old_table_held)
{ av::MockPlatform   platform;
  D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake, platform);
  ScriptedDevice     device;
  ScriptedQueue      queue;
  for (std::uint32_t id = 1u; id <= 64u; id++) {
      CHECK(progress.executed(id, &queue, &device));
      CHECK(progress.invariants_hold());
  }
  CHECK(progress.marked() == 64u);
  for (std::uint32_t index = 0u; index < 64u; index++) device.fences[index].completed = index + 1u;
  CHECK(progress.reached() == 64u);
  device.fences[40].completed = 0u;
  CHECK(progress.reached() == 0u);
  progress.clear();
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(d3d12_queue_progress_executed, gives_the_fence_back_when_the_queue_cannot_be_kept)
{ av::MockPlatform   platform;
  D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake, platform);
  ScriptedDevice     device;
  ScriptedQueue      queue;
  platform.fail_allocation_at(1u);
  CHECK(!progress.executed(1u, &queue, &device));
  CHECK_EQ(device.made, 1u);
  CHECK_EQ(device.fences[0].references, 0u);
  CHECK(progress.marked() == 0u);
  CHECK(progress.invariants_hold());
  CHECK(progress.executed(1u, &queue, &device));
  CHECK(queue.signalled == &device.fences[1]);
  progress.clear(); }

TEST(d3d12_queue_progress_forget, lets_one_queue_s_fence_go_and_clear_every_fence_while_marks_keep_rising)
{ av::MockPlatform   platform;
  D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake, platform);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy;
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(progress.executed(2u, &copy, &device));
  progress.forget(1u);
  progress.forget(7u);
  CHECK_EQ(device.fences[0].references, 0u);
  CHECK_EQ(device.fences[1].references, 1u);
  device.fences[1].completed = 2u;
  CHECK(progress.reached() == 2u);
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(direct.signalled == &device.fences[2]);
  CHECK(direct.signalled_value == 3u);
  progress.clear();
  CHECK_EQ(device.fences[1].references, 0u);
  CHECK_EQ(device.fences[2].references, 0u);
  CHECK(progress.marked() == 3u);
  CHECK(progress.reached() == 3u);
  CHECK(progress.executed(2u, &copy, &device));
  CHECK(copy.signalled_value == 4u);
  CHECK(progress.invariants_hold()); }

TEST(d3d12_queue_progress_mark_all, signals_each_queue_s_own_fence_and_is_reached_only_once_all_are)
{ av::MockPlatform   platform;
  D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake, platform);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy;
  CHECK(progress.mark_all());
  CHECK(progress.marked() == 0u);
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(progress.executed(2u, &copy, &device));
  device.fences[0].completed = 1u;
  device.fences[1].completed = 2u;
  CHECK(progress.mark_all());
  CHECK(direct.signalled == &device.fences[0]);
  CHECK(direct.signalled_value == 3u);
  CHECK(copy.signalled == &device.fences[1]);
  CHECK(copy.signalled_value == 4u);
  CHECK(progress.marked() == 4u);
  device.fences[1].completed = 4u;
  CHECK(progress.reached() == 1u);
  device.fences[0].completed = 3u;
  CHECK(progress.reached() == progress.marked());
  copy.refuse_signal = true;
  CHECK(!progress.mark_all());
  CHECK(direct.signalled_value == 5u);
  CHECK(progress.marked() == 5u);
  CHECK(progress.reached() == 3u);
  progress.forget(1u);
  copy.refuse_signal = false;
  direct.signalled_value = 0u;
  CHECK(progress.mark_all());
  CHECK(copy.signalled_value == 6u);
  CHECK(direct.signalled_value == 0u);
  CHECK(progress.invariants_hold()); }

TEST(d3d12_queue_progress_log_stuck, names_each_queue_with_its_signalled_and_completed_marks)
{ av::MockPlatform   platform;
  D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake, platform);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy;
  av::RecordingLog   log;
  av::DriverLog      lines(log, "d3d12: ");
  progress.log_stuck(lines);
  CHECK_EQ(log.lines(), 0u);
  CHECK(progress.executed(5u, &direct, &device));
  CHECK(progress.executed(9u, &copy, &device));
  device.fences[0].completed = 1u;
  device.fences[1].completed = 0u;
  progress.log_stuck(lines);
  CHECK_EQ(log.lines(), 2u);
  CHECK(std::strcmp(log.line(0u), "d3d12: queue 5 progress signalled 1 completed 1") == 0);
  CHECK(std::strcmp(log.line(1u), "d3d12: queue 9 progress signalled 2 completed 0") == 0);
  progress.clear();
  progress.log_stuck(lines);
  CHECK_EQ(log.lines(), 2u);
  CHECK(progress.invariants_hold()); }
