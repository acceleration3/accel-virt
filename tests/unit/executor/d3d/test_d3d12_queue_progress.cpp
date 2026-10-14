/* How far the host GPU has got with what a session's Direct3D 12 queues were
 * given: each execute's queue signals its own fence with the session's next
 * mark, reached() is the highest mark everything below has passed, and an
 * execute waits on the GPU for what sibling queues were given first. The
 * queue, its fences and the device are scripted stand-ins for vkd3d-proton's,
 * so which fence was signalled and waited on with which value is what is
 * judged, and a fence completes when the test says the GPU got there. */
#include "core/log/driver_log.hpp"
#include "executor/d3d/d3d12_fence_wake.hpp"
#include "executor/d3d/d3d12_queue_progress.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

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

/* A queue that records what it was told to signal and wait for. */
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

/* A device that makes its fences out of a fixed set, and can refuse. */
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
        if (refuse_fence || made == 20u) return E_OUTOFMEMORY;
        fences[made].completed = initial;
        *fence = &fences[made++];
        return S_OK;
    }

    ScriptedFence fences[20];
    std::uint32_t made = 0u;
    bool          refuse_fence = false;
};

}  // namespace

TEST(d3d12_queue_progress_executed, each_execute_signals_its_queue_s_fence_with_the_next_mark)
{ D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake);
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
  CHECK(progress.executed(1u, &direct, &device));   /* the queue's fence, made once */
  CHECK(direct.signalled == &device.fences[0]);
  CHECK(direct.signalled_value == 3u);
  CHECK_EQ(device.made, 2u);
  CHECK(progress.marked() == 3u);
  CHECK(progress.invariants_hold()); }

/* Everything at or below reached() has finished: a queue behind its last
 * signal holds the answer at its completed value, one caught up holds nothing. */
TEST(d3d12_queue_progress_reached, is_held_by_the_queue_furthest_behind_its_last_signal)
{ D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy;
  CHECK(progress.executed(1u, &direct, &device));   /* mark 1 on direct */
  CHECK(progress.executed(2u, &copy, &device));     /* mark 2 on copy */
  CHECK(progress.executed(1u, &direct, &device));   /* mark 3 on direct */
  CHECK(progress.reached() == 0u);
  device.fences[1].completed = 2u;                  /* copy caught up */
  CHECK(progress.reached() == 0u);
  device.fences[0].completed = 1u;                  /* direct passed 1 of 3 */
  CHECK(progress.reached() == 1u);
  device.fences[0].completed = 3u;
  CHECK(progress.reached() == 3u);
  CHECK(progress.reached() == progress.marked()); }

TEST(d3d12_queue_progress_after_siblings, waits_on_every_other_queue_still_behind_and_never_its_own)
{ D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy, compute;
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(progress.executed(2u, &copy, &device));
  CHECK(progress.executed(3u, &compute, &device));
  device.fences[2].completed = 3u;   /* compute finished its mark */
  CHECK(progress.after_siblings(1u, &direct));
  CHECK_EQ(direct.waits, 1u);        /* copy alone: its own and a finished one are not waited on */
  CHECK(direct.waited[0] == &device.fences[1]);
  CHECK(direct.waited_value[0] == 2u);
  device.fences[1].completed = 2u;
  CHECK(progress.after_siblings(1u, &direct));
  CHECK_EQ(direct.waits, 1u);
  CHECK(progress.after_siblings(9u, &direct));   /* a queue never executed waits on every one behind */
  CHECK_EQ(direct.waits, 2u);
  CHECK(direct.waited[1] == &device.fences[0]);
  device.fences[0].completed = 1u;
  CHECK(progress.after_siblings(9u, &direct));   /* and none once all have finished */
  CHECK_EQ(direct.waits, 2u);
  device.fences[0].completed = 0u;
  CHECK(progress.after_siblings(2u, &copy));
  CHECK_EQ(copy.waits, 1u);
  CHECK(copy.waited[0] == &device.fences[0]);
  CHECK(copy.waited_value[0] == 1u); }

TEST(d3d12_queue_progress_after_siblings, a_refused_wait_or_no_queue_is_refused)
{ D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy;
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(!progress.after_siblings(2u, nullptr));
  copy.refuse_wait = true;
  CHECK(!progress.after_siblings(2u, &copy));
  CHECK(progress.after_siblings(1u, &copy));   /* nothing to wait on: no wait to refuse */ }

TEST(d3d12_queue_progress_executed, refuses_what_cannot_be_made_or_signalled_and_a_full_table)
{ D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake);
  ScriptedDevice     device;
  ScriptedQueue      queue;
  CHECK(!progress.executed(1u, nullptr, &device));
  CHECK(!progress.executed(1u, &queue, nullptr));
  device.refuse_fence = true;
  CHECK(!progress.executed(1u, &queue, &device));
  device.refuse_fence = false;
  queue.refuse_signal = true;
  CHECK(!progress.executed(1u, &queue, &device));
  CHECK(progress.marked() == 0u);   /* a refused signal leaves no mark */
  queue.refuse_signal = false;
  CHECK(progress.executed(1u, &queue, &device));
  CHECK(progress.marked() == 1u);
  for (std::uint32_t id = 2u; id <= D3d12QueueProgress::capacity; id++) CHECK(progress.executed(id, &queue, &device));
  CHECK(!progress.executed(D3d12QueueProgress::capacity + 1u, &queue, &device));
  CHECK(progress.invariants_hold()); }

TEST(d3d12_queue_progress_forget, lets_one_queue_s_fence_go_and_clear_every_fence_while_marks_keep_rising)
{ D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy;
  CHECK(progress.executed(1u, &direct, &device));
  CHECK(progress.executed(2u, &copy, &device));
  progress.forget(1u);
  progress.forget(7u);
  CHECK_EQ(device.fences[0].references, 0u);
  CHECK_EQ(device.fences[1].references, 1u);
  /* The forgotten queue no longer holds reached() back. */
  device.fences[1].completed = 2u;
  CHECK(progress.reached() == 2u);
  CHECK(progress.executed(1u, &direct, &device));   /* a new fence for it */
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

/* A drain marks every queue that executed on its own fence, so it is over
 * only once each has caught up - an idle queue finishing first reaches
 * nothing for a busy one. A queue refusing its signal is reported and leaves
 * no mark, and the others are marked all the same. */
TEST(d3d12_queue_progress_mark_all, signals_each_queue_s_own_fence_and_is_reached_only_once_all_are)
{ D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy;
  CHECK(progress.mark_all());                       /* nothing executed: nothing to mark */
  CHECK(progress.marked() == 0u);
  CHECK(progress.executed(1u, &direct, &device));   /* mark 1 */
  CHECK(progress.executed(2u, &copy, &device));     /* mark 2 */
  device.fences[0].completed = 1u;
  device.fences[1].completed = 2u;
  CHECK(progress.mark_all());
  CHECK(direct.signalled == &device.fences[0]);
  CHECK(direct.signalled_value == 3u);
  CHECK(copy.signalled == &device.fences[1]);
  CHECK(copy.signalled_value == 4u);
  CHECK(progress.marked() == 4u);
  device.fences[1].completed = 4u;                  /* the idle queue first */
  CHECK(progress.reached() == 1u);
  device.fences[0].completed = 3u;
  CHECK(progress.reached() == progress.marked());
  copy.refuse_signal = true;
  CHECK(!progress.mark_all());
  CHECK(direct.signalled_value == 5u);
  CHECK(progress.marked() == 5u);
  CHECK(progress.reached() == 3u);                  /* direct behind at 3; copy, unmarked, holds nothing */
  progress.forget(1u);
  copy.refuse_signal = false;
  direct.signalled_value = 0u;
  CHECK(progress.mark_all());
  CHECK(copy.signalled_value == 6u);                /* the forgotten queue is not marked */
  CHECK(direct.signalled_value == 0u);
  CHECK(progress.invariants_hold()); }

/* A drain that never finishes says which queue it waits for: each queue's
 * guest id, the mark it last signalled and what its fence completed, one line
 * a queue, and nothing for a session with no queue. */
TEST(d3d12_queue_progress_log_stuck, names_each_queue_with_its_signalled_and_completed_marks)
{ D3d12FenceWake     wake;
  D3d12QueueProgress progress(wake);
  ScriptedDevice     device;
  ScriptedQueue      direct, copy;
  av::RecordingLog   log;
  av::DriverLog      lines(log, "d3d12: ");
  progress.log_stuck(lines);
  CHECK_EQ(log.lines(), 0u);
  CHECK(progress.executed(5u, &direct, &device));   /* mark 1 */
  CHECK(progress.executed(9u, &copy, &device));     /* mark 2 */
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
