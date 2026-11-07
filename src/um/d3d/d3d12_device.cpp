#include "um/d3d/d3d12_device.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/keyed_log.hpp"
#include "icd/d3d/session_refusal.hpp"
#include "icd/common/pieces.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "proto/escape/present_point_record.hpp"
#include "um/d3d/d3d12_host_caps.hpp"
#include "um/d3d/query_results.hpp"
#include "um/d3d/d3d_session.hpp"

namespace av::d3d12 {

namespace {
constexpr uint32_t list_direct = 0u;
constexpr uint32_t list_bundle = 1u;
constexpr uint32_t list_compute = 2u;
constexpr uint32_t list_copy = 3u;
constexpr uint64_t verdict_every = 64u;

uint32_t list_type_of(D3D12DDI_COMMAND_QUEUE_FLAGS flags)
{
    AV_SCOPE("list_type_of");
    if ((flags & D3D12DDI_COMMAND_QUEUE_FLAG_3D) != 0) return list_direct;
    if ((flags & D3D12DDI_COMMAND_QUEUE_FLAG_COMPUTE) != 0) return list_compute;
    if ((flags & D3D12DDI_COMMAND_QUEUE_FLAG_COPY) != 0) return list_copy;
    return list_direct;
}

}

D3d12Device::D3d12Device(BlockSource &memory, d3d::D3dSession &session, KeyedLog &log,
                         const D3d12HostCaps &caps,
                         void *const &list_table, d3d::D3dCallTrace &trace)
    : session_(session), lines_(log), refused_(lines_), caps_(caps), list_table_(list_table), runtime_{}, core_(nullptr), kernel_(nullptr), lender_(session), memories_(memory, session, log), follower_(), residency_(), channel_(), addresses_(), commitment_(),
      recorder_(*this), null_resource_{}, loss_(lines_), library_blob_(), trace_(trace),
      context_number_(trace.number_context())
{
    AV_SCOPE("D3d12Device::D3d12Device");
    null_resource_.depth_or_array = 1u;
    null_resource_.sample_count = 1u;
}

D3d12Device::~D3d12Device()
{
    AV_SCOPE("D3d12Device::~D3d12Device");
    follower_.destroy();
    residency_.destroy();
    if (!channel_.constructed()) return;
    static_cast<void>(channel_.object().flush());
    channel_.destroy();
    session_.leave();
}

HRESULT D3d12Device::initialize(uint32_t adapter, const D3D12DDIARG_CREATEDEVICE_0109 &create)
{
    AV_SCOPE("D3d12Device::initialize");
    runtime_ = create.hRTDevice;
    core_ = create.p12UMCallbacks_0050;
    kernel_ = create.pKTCallbacks;
    lender_.bind(create.pKTCallbacks, runtime_.handle);
    const HRESULT joined = session_.join(adapter);
    if (FAILED(joined)) return joined;
    if (create.pKTCallbacks != nullptr) {
        static_cast<void>(follower_.construct(*create.pKTCallbacks, runtime_.handle, session_.transport()));
    }
    if (core_ != nullptr) static_cast<void>(residency_.construct(*core_, runtime_));
    static_cast<void>(channel_.construct(session_.stream(), session_.lock(), 0u));
    commitment_.learn(caps_.local_budget());
    uint32_t hresult = 1u;
    if (!d3d12proto::av_d3d12_OpenDevice12(channel_.object(), 0u, &hresult) || hresult != 0u) {
        lines_.write("the host made no Direct3D 12 device");
        channel_.destroy();
        const bool exhausted = session_.exhausted();
        session_.leave();
        return d3d::SessionRefusal(exhausted).for_d3d();
    }
    return S_OK;
}

HRESULT D3d12Device::answered(const char *slot, HRESULT hresult)
{
    AV_SCOPE("D3d12Device::answered");
    if (FAILED(hresult) && hresult != E_PENDING) refused_.failed(slot, hresult);
    return hresult;
}

void D3d12Device::unsupported(const char *slot)
{
    AV_SCOPE("D3d12Device::unsupported");
    refused_.unserved(slot);
}

d3d::D3dDeviceChannel &D3d12Device::wire()
{
    AV_SCOPE("D3d12Device::wire");
    return channel_.object();
}

uint32_t D3d12Device::id_of(const void *block) const
{
    AV_SCOPE("D3d12Device::id_of");
    return block == nullptr ? 0u : static_cast<const RecordHeader *>(block)->stamp.id();
}

void D3d12Device::lost(const char *what)
{
    AV_SCOPE("D3d12Device::lost");
    const bool known = loss_.lost();
    loss_.lose(session_.fault_code());
    static_cast<void>(loss_.refuses(what));
    if (!known && core_ != nullptr) core_->pfnSetErrorCb(runtime_, D3DDDIERR_DEVICEREMOVED);
}

uint32_t D3d12Device::next_id()
{
    AV_SCOPE("D3d12Device::next_id");
    return session_.next_id();
}

void D3d12Device::stamp(RecordHeader &header, uint32_t tag, uint32_t id)
{
    AV_SCOPE("D3d12Device::stamp");
    header.stamp = d3d::RecordStamp(tag, id);
    header.device = this;
    header.recorder = &recorder_;
}

bool D3d12Device::sent(bool sent_ok, const char *what)
{
    AV_SCOPE("D3d12Device::sent");
    if (!sent_ok) lost(what);
    return sent_ok;
}

VOID D3d12Device::check_format_support(DXGI_FORMAT format, UINT *support)
{
    AV_SCOPE("D3d12Device::check_format_support");
    if (support != nullptr) *support = caps_.format_support(format);
}

VOID D3d12Device::check_multisample_quality_levels(DXGI_FORMAT format, UINT samples,
                                                   D3D12DDI_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags, UINT *levels)
{
    AV_SCOPE("D3d12Device::check_multisample_quality_levels");
    if (levels == nullptr) return;
    *levels = (flags & D3D12DDI_MULTISAMPLE_QUALITY_LEVEL_FLAG_TILED_RESOURCE) != 0 ? 0u
                                                                                   : caps_.quality_levels(format, samples);
}

UINT D3d12Device::get_implicit_physical_adapter_mask()
{
    AV_SCOPE("D3d12Device::get_implicit_physical_adapter_mask");
    return 1u;
}

UINT D3d12Device::get_present_private_driver_data_size(const D3D12DDIARG_PRESENT_0001 *)
{
    AV_SCOPE("D3d12Device::get_present_private_driver_data_size");
    return static_cast<UINT>(PresentPointRecord::encoded_size);
}

VOID D3d12Device::query_node_map(UINT count, UINT *map)
{
    AV_SCOPE("D3d12Device::query_node_map");
    for (UINT node = 0u; map != nullptr && node < count; node++) map[node] = node;
}

HRESULT D3d12Device::enumerate_meta_commands(UINT *count, D3D12DDIARG_META_COMMAND_DESC *)
{
    AV_SCOPE("D3d12Device::enumerate_meta_commands");
    if (count == nullptr) return E_INVALIDARG;
    *count = 0u;
    return S_OK;
}

VOID D3d12Device::get_debug_allocation_info(D3D12DDI_HANDLE_AND_TYPE, UINT *virtual_addresses,
                                            D3D12DDI_DEBUG_VIRTUAL_ADDRESS_ALLOCATION_INFO_0012 *,
                                            UINT *kernel_allocations, D3D12DDI_DEBUG_KMT_ALLOCATION_INFO_0014 *)
{
    AV_SCOPE("D3d12Device::get_debug_allocation_info");
    if (virtual_addresses != nullptr) *virtual_addresses = 0u;
    if (kernel_allocations != nullptr) *kernel_allocations = 0u;
}

D3D12DDI_DRIVER_MATCHING_IDENTIFIER_STATUS
D3d12Device::check_driver_matching_identifier(D3D12DDI_SERIALIZED_DATA_TYPE,
                                              const D3D12DDI_SERIALIZED_DATA_DRIVER_MATCHING_IDENTIFIER_0054 *)
{
    AV_SCOPE("D3d12Device::check_driver_matching_identifier");
    return D3D12DDI_DRIVER_MATCHING_IDENTIFIER_UNSUPPORTED_TYPE;
}

HRESULT D3d12Device::get_supported_extended_features(D3D12DDI_FEATURE_0020, UINT32 *count, D3D12DDI_FEATURE_0020 *)
{
    AV_SCOPE("D3d12Device::get_supported_extended_features");
    if (count == nullptr) return E_INVALIDARG;
    *count = 0u;
    return S_OK;
}

HRESULT D3d12Device::get_supported_extended_feature_versions(D3D12DDI_FEATURE_0020, UINT32 *count, UINT32 *)
{
    AV_SCOPE("D3d12Device::get_supported_extended_feature_versions");
    if (count == nullptr) return E_INVALIDARG;
    *count = 0u;
    return S_OK;
}

HRESULT D3d12Device::enable_extended_feature(D3D12DDI_FEATURE_0020, UINT32)
{
    AV_SCOPE("D3d12Device::enable_extended_feature");
    return E_INVALIDARG;
}

HRESULT D3d12Device::set_extended_feature_callbacks(D3D12DDI_TABLE_TYPE, const void *, SIZE_T)
{
    AV_SCOPE("D3d12Device::set_extended_feature_callbacks");
    return S_OK;
}

HRESULT D3d12Device::make_resident(D3D12DDIARG_MAKERESIDENT_0001 *args)
{
    AV_SCOPE("D3d12Device::make_resident");
    if (args == nullptr || !residency_.constructed()) return E_INVALIDARG;
    return residency_.object().make_resident(*args);
}

HRESULT D3d12Device::evict(const D3D12DDIARG_EVICT *args)
{
    AV_SCOPE("D3d12Device::evict");
    if (args == nullptr || !residency_.constructed()) return E_INVALIDARG;
    return residency_.object().evict(*args);
}

SIZE_T D3d12Device::calc_private_command_queue_size(const D3D12DDIARG_CREATECOMMANDQUEUE_0050 *)
{
    AV_SCOPE("D3d12Device::calc_private_command_queue_size");
    return sizeof(QueueRecord);
}

HRESULT D3d12Device::create_command_queue(const D3D12DDIARG_CREATECOMMANDQUEUE_0050 *args,
                                          D3D12DDI_HCOMMANDQUEUE handle, D3D12DDI_HRTCOMMANDQUEUE runtime)
{
    AV_SCOPE("D3d12Device::create_command_queue");
    if (args == nullptr || handle.pDrvPrivate == nullptr || core_ == nullptr) return E_INVALIDARG;
    QueueRecord &queue = *static_cast<QueueRecord *>(handle.pDrvPrivate);
    queue = QueueRecord{};
    stamp(queue.header, QueueRecord::tag, next_id());
    queue.runtime = runtime;
    queue.type = list_type_of(args->QueueFlags);
    D3DDDICB_CREATECONTEXTVIRTUAL context = {};
    context.NodeOrdinal = 0u;
    context.EngineAffinity = 1u;
    if (FAILED(core_->pfnCreateContextVirtualCb(runtime, &context))) {
        lines_.write("the kernel refused a Direct3D 12 queue's context");
        return E_OUTOFMEMORY;
    }
    queue.context = context.hContext;
    if (!sent(d3d12proto::av_d3d12_CreateCommandQueue12(wire(), queue.header.stamp.id(), queue.type, 0u, 0u),
              "CreateCommandQueue12"))
        return E_OUTOFMEMORY;
    return S_OK;
}

VOID D3d12Device::destroy_command_queue(D3D12DDI_HCOMMANDQUEUE handle)
{
    AV_SCOPE("D3d12Device::destroy_command_queue");
    QueueRecord &queue = *static_cast<QueueRecord *>(handle.pDrvPrivate);
    static_cast<void>(sent(d3d12proto::av_d3d12_Destroy12(wire(), queue.header.stamp.id()), "Destroy12"));
    queue = QueueRecord{};
}

VOID D3d12Device::execute_command_lists(D3D12DDI_HCOMMANDQUEUE handle, UINT count, const D3D12DDI_HCOMMANDLIST *lists)
{
    AV_SCOPE("D3d12Device::execute_command_lists");
    constexpr UINT most_lists = 64u;
    QueueRecord &queue = *static_cast<QueueRecord *>(handle.pDrvPrivate);
    icd::Pieces pieces(lists == nullptr ? 0u : count, most_lists);
    while (pieces.next()) {
        uint32_t   ids[most_lists] = {};
        const auto batch = static_cast<UINT>(pieces.count());
        for (UINT index = 0u; index < batch; index++) ids[index] = id_of(lists[pieces.offset() + index].pDrvPrivate);
        if (!sent(d3d12proto::av_d3d12_ExecuteCommandLists12(wire(), queue.header.stamp.id(), batch, ids),
                  "ExecuteCommandLists12"))
            return;
    }
    queue.executed++;
    if (wire().flush() != Status::ok) {
        lost("the queue's work could not be sent");
        return;
    }
    trim_memory();
    if (!follow(queue)) return;
    if (queue.executed % verdict_every != 0u) return;
    uint32_t removed = 0u;
    if (!sent(d3d12proto::av_d3d12_Finish12(wire(), &removed), "Finish12")) return;
    if (removed != 0u) lost("the host removed the device");
}

bool D3d12Device::follow(const QueueRecord &queue)
{
    AV_SCOPE("D3d12Device::follow");
    const HRESULT followed = follower_.constructed() ? follower_.object().follow(queue.context) : E_FAIL;
    if (SUCCEEDED(followed)) return true;
    refused_.failed("pfnSubmitCommandCb", followed);
    lost("the queue's context refused its packet");
    return false;
}

VOID D3d12Device::signal_fence(D3D12DDI_HCOMMANDQUEUE, D3D12DDIARG_FENCE_OPERATION *)
{
    AV_SCOPE("D3d12Device::signal_fence");
    unsupported("pfnSignalFence");
}

VOID D3d12Device::wait_for_fence(D3D12DDI_HCOMMANDQUEUE, D3D12DDIARG_FENCE_OPERATION *)
{
    AV_SCOPE("D3d12Device::wait_for_fence");
    unsupported("pfnWaitForFence");
}

SIZE_T D3d12Device::calc_private_command_pool_size(const D3D12DDIARG_CREATE_COMMAND_POOL_0040 *)
{
    AV_SCOPE("D3d12Device::calc_private_command_pool_size");
    return sizeof(PoolRecord);
}

HRESULT D3d12Device::create_command_pool(const D3D12DDIARG_CREATE_COMMAND_POOL_0040 *, D3D12DDI_HCOMMANDPOOL_0040 handle)
{
    AV_SCOPE("D3d12Device::create_command_pool");
    if (handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    PoolRecord &pool = *static_cast<PoolRecord *>(handle.pDrvPrivate);
    pool = PoolRecord{};
    stamp(pool.header, PoolRecord::tag, next_id());
    return S_OK;
}

VOID D3d12Device::destroy_command_pool(D3D12DDI_HCOMMANDPOOL_0040 handle)
{
    AV_SCOPE("D3d12Device::destroy_command_pool");
    PoolRecord &pool = *static_cast<PoolRecord *>(handle.pDrvPrivate);
    if (pool.made != 0u) static_cast<void>(sent(d3d12proto::av_d3d12_Destroy12(wire(), pool.header.stamp.id()), "Destroy12"));
    pool = PoolRecord{};
}

VOID D3d12Device::reset_command_pool(D3D12DDI_HCOMMANDPOOL_0040 handle)
{
    AV_SCOPE("D3d12Device::reset_command_pool");
    const PoolRecord &pool = *static_cast<const PoolRecord *>(handle.pDrvPrivate);
    if (pool.made == 0u) return;
    static_cast<void>(sent(d3d12proto::av_d3d12_ResetCommandAllocator12(wire(), pool.header.stamp.id()),
                           "ResetCommandAllocator12"));
}

SIZE_T D3d12Device::calc_private_command_recorder_size(const D3D12DDIARG_CREATE_COMMAND_RECORDER_0040 *)
{
    AV_SCOPE("D3d12Device::calc_private_command_recorder_size");
    return sizeof(RecorderRecord);
}

HRESULT D3d12Device::create_command_recorder(const D3D12DDIARG_CREATE_COMMAND_RECORDER_0040 *,
                                             D3D12DDI_HCOMMANDRECORDER_0040 handle)
{
    AV_SCOPE("D3d12Device::create_command_recorder");
    if (handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    RecorderRecord &recorder = *static_cast<RecorderRecord *>(handle.pDrvPrivate);
    recorder = RecorderRecord{};
    stamp(recorder.header, RecorderRecord::tag, 0u);
    return S_OK;
}

VOID D3d12Device::destroy_command_recorder(D3D12DDI_HCOMMANDRECORDER_0040 handle)
{
    AV_SCOPE("D3d12Device::destroy_command_recorder");
    *static_cast<RecorderRecord *>(handle.pDrvPrivate) = RecorderRecord{};
}

VOID D3d12Device::command_recorder_set_command_pool_as_target(D3D12DDI_HCOMMANDRECORDER_0040 recorder,
                                                              D3D12DDI_HCOMMANDPOOL_0040 pool)
{
    AV_SCOPE("D3d12Device::command_recorder_set_command_pool_as_target");
    static_cast<RecorderRecord *>(recorder.pDrvPrivate)->pool = static_cast<PoolRecord *>(pool.pDrvPrivate);
}

SIZE_T D3d12Device::calc_private_command_list_size(const D3D12DDIARG_CREATE_COMMAND_LIST_0040 *)
{
    AV_SCOPE("D3d12Device::calc_private_command_list_size");
    return sizeof(ListRecord);
}

HRESULT D3d12Device::create_command_list(const D3D12DDIARG_CREATE_COMMAND_LIST_0040 *args, D3D12DDI_HCOMMANDLIST handle,
                                         D3D12DDI_HRTCOMMANDLIST runtime_list)
{
    AV_SCOPE("D3d12Device::create_command_list");
    if (args == nullptr || handle.pDrvPrivate == nullptr || core_ == nullptr || list_table_ == nullptr)
        return E_INVALIDARG;
    core_->pfnSetCommandListDDITableCb(runtime_list, D3D12DDI_HRTTABLE{list_table_});
    ListRecord &list = *static_cast<ListRecord *>(handle.pDrvPrivate);
    list = ListRecord{};
    stamp(list.header, ListRecord::tag, next_id());
    list.type = args->Type == D3D12DDI_COMMAND_LIST_TYPE_BUNDLE ? list_bundle : list_type_of(args->QueueFlags);
    return S_OK;
}

VOID D3d12Device::destroy_command_list(D3D12DDI_HCOMMANDLIST handle)
{
    AV_SCOPE("D3d12Device::destroy_command_list");
    ListRecord &list = *static_cast<ListRecord *>(handle.pDrvPrivate);
    if (list.made != 0u) static_cast<void>(sent(d3d12proto::av_d3d12_Destroy12(wire(), list.header.stamp.id()), "Destroy12"));
    list = ListRecord{};
}

SIZE_T D3d12Device::calc_private_fence_size(const D3D12DDIARG_CREATE_FENCE *)
{
    AV_SCOPE("D3d12Device::calc_private_fence_size");
    return sizeof(ObjectRecord);
}

HRESULT D3d12Device::create_fence(D3D12DDI_HFENCE handle, const D3D12DDIARG_CREATE_FENCE *)
{
    AV_SCOPE("D3d12Device::create_fence");
    if (handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    ObjectRecord &fence = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    fence = ObjectRecord{};
    stamp(fence.header, ObjectRecord::tag, 0u);
    return S_OK;
}

VOID D3d12Device::destroy_fence(D3D12DDI_HFENCE handle)
{
    AV_SCOPE("D3d12Device::destroy_fence");
    *static_cast<ObjectRecord *>(handle.pDrvPrivate) = ObjectRecord{};
}

SIZE_T D3d12Device::calc_private_query_heap_size(const D3D12DDIARG_CREATE_QUERY_HEAP_0001 *)
{
    AV_SCOPE("D3d12Device::calc_private_query_heap_size");
    return sizeof(ObjectRecord);
}

HRESULT D3d12Device::create_query_heap(const D3D12DDIARG_CREATE_QUERY_HEAP_0001 *args, D3D12DDI_HQUERYHEAP handle)
{
    AV_SCOPE("D3d12Device::create_query_heap");
    if (args == nullptr || handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    ObjectRecord &heap = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    heap = ObjectRecord{};
    stamp(heap.header, ObjectRecord::tag, next_id());
    heap.kind = static_cast<uint32_t>(args->Type);
    if (!sent(d3d12proto::av_d3d12_CreateQueryHeap12(wire(), heap.header.stamp.id(), heap.kind, args->Count),
              "CreateQueryHeap12"))
        return E_OUTOFMEMORY;
    return S_OK;
}

VOID D3d12Device::destroy_query_heap(D3D12DDI_HQUERYHEAP handle)
{
    AV_SCOPE("D3d12Device::destroy_query_heap");
    ObjectRecord &heap = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    static_cast<void>(sent(d3d12proto::av_d3d12_Destroy12(wire(), heap.header.stamp.id()), "Destroy12"));
    heap = ObjectRecord{};
}

}
