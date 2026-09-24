/* The device itself: joining the session, the queues and what records into
 * them, fences, queries, and the slots that answer a fixed fact. */
#include "um/d3d/d3d12_device.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/d3d/d3d12_heap_arena.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "um/d3d/d3d12_host_caps.hpp"
#include "um/d3d/d3d_session.hpp"

namespace av::d3d12 {

namespace {

/* D3D12_COMMAND_LIST_TYPE. */
constexpr uint32_t list_direct = 0u;
constexpr uint32_t list_bundle = 1u;
constexpr uint32_t list_compute = 2u;
constexpr uint32_t list_copy = 3u;
/* What of the session's arena is lent to bulk data (pipeline code, root
 * constants too large to cross inline); the rest holds heaps, up to the
 * query result slots the session keeps at its end. */
constexpr uint64_t lent_bytes = uint64_t{32} << 20u;
constexpr uint64_t query_slot_bytes = uint64_t{64} << 10u;

uint32_t list_type_of(D3D12DDI_COMMAND_QUEUE_FLAGS flags)
{
    AV_SCOPE("list_type_of");
    if ((flags & D3D12DDI_COMMAND_QUEUE_FLAG_COMPUTE) != 0) return list_compute;
    if ((flags & D3D12DDI_COMMAND_QUEUE_FLAG_COPY) != 0) return list_copy;
    return list_direct;
}

}  // namespace

D3d12Device::D3d12Device(d3d::D3dSession &session, d3d::D3d12HeapArena &heaps, icd::LogSink &log,
                         const D3d12HostCaps &caps)
    : session_(session), heaps_(heaps), log_(log), caps_(caps), runtime_{}, core_(nullptr), channel_(), addresses_(),
      recorder_(*this), null_resource_{}, lost_(false), logged_{}
{
    AV_SCOPE("D3d12Device::D3d12Device");
    null_resource_.depth_or_array = 1u;
    null_resource_.sample_count = 1u;
}

D3d12Device::~D3d12Device()
{
    AV_SCOPE("D3d12Device::~D3d12Device");
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
    const HRESULT joined = session_.join(adapter);
    if (FAILED(joined)) return joined;
    static_cast<void>(channel_.construct(session_.stream(), session_.lock(), 0u));
    /* A session no heap lives in yet lends only its first part, whichever
     * device of it arrives first: the arena was just opened, or every heap in
     * it has gone. */
    uint8_t *const  view = session_.arena_view();
    const uint64_t  arena = session_.arena_bytes();
    if (view != nullptr && arena > lent_bytes + query_slot_bytes && heaps_.placed() == 0u &&
        session_.blobs().used() == 0u) {
        session_.blobs().lend(view, static_cast<size_t>(lent_bytes));
        heaps_.take(lent_bytes, arena - query_slot_bytes);
    }
    uint32_t           hresult = 1u;
    d3d12proto::Caps12 host = {};
    if (!d3d12proto::rvk_d3d12_OpenDevice12(channel_.object(), 0u, &hresult, &host) || hresult != 0u) {
        log_.write("rvkumd: the host made no Direct3D 12 device");
        channel_.destroy();
        session_.leave();
        return E_FAIL;
    }
    return S_OK;
}

void D3d12Device::unsupported(const char *slot)
{
    AV_SCOPE("D3d12Device::unsupported");
    uint32_t length = 0u;
    uint32_t hash = 2166136261u;
    for (; slot[length] != '\0'; length++) hash = (hash ^ static_cast<uint8_t>(slot[length])) * 16777619u;
    const uint32_t bit = hash % 256u;
    uint64_t      &word = logged_[bit / 64u];
    const uint64_t mask = uint64_t{1} << (bit % 64u);
    if ((word & mask) != 0u) return;
    word |= mask;
    char       line[128] = {};
    TextBuffer text(line, sizeof(line));
    text.append("rvkumd: Direct3D 12 slot not implemented: ");
    text.append(slot);
    log_.write(text.text());
}

d3d::D3dDeviceChannel &D3d12Device::wire()
{
    AV_SCOPE("D3d12Device::wire");
    return channel_.object();
}

uint32_t D3d12Device::id_of(const void *block) const
{
    AV_SCOPE("D3d12Device::id_of");
    return block == nullptr ? 0u : static_cast<const RecordHeader *>(block)->id;
}

void D3d12Device::lost(const char *what)
{
    AV_SCOPE("D3d12Device::lost");
    if (lost_) return;
    lost_ = true;
    char       line[128] = {};
    TextBuffer text(line, sizeof(line));
    text.append("rvkumd: Direct3D 12 device lost: ");
    text.append(what);
    log_.write(text.text());
    if (core_ != nullptr) core_->pfnSetErrorCb(runtime_, D3DDDIERR_DEVICEREMOVED);
}

uint32_t D3d12Device::next_id()
{
    AV_SCOPE("D3d12Device::next_id");
    return session_.next_id();
}

void D3d12Device::stamp(RecordHeader &header, uint32_t magic, uint32_t id)
{
    AV_SCOPE("D3d12Device::stamp");
    header.magic = magic;
    header.id = id;
    header.device = this;
    header.recorder = &recorder_;
}

bool D3d12Device::sent(bool sent_ok, const char *what)
{
    AV_SCOPE("D3d12Device::sent");
    if (!sent_ok) lost(what);
    return sent_ok;
}

/* ---------------------------------------------------------------- formats */

VOID D3d12Device::check_format_support(DXGI_FORMAT format, UINT *support)
{
    AV_SCOPE("D3d12Device::check_format_support");
    if (support != nullptr) *support = caps_.format_support(format);
}

/* No multisampling yet: one sample of a format that is there at all, and no
 * quality level at any other count. */
VOID D3d12Device::check_multisample_quality_levels(DXGI_FORMAT format, UINT samples,
                                                   D3D12DDI_MULTISAMPLE_QUALITY_LEVEL_FLAGS, UINT *levels)
{
    AV_SCOPE("D3d12Device::check_multisample_quality_levels");
    if (levels == nullptr) return;
    *levels = samples == 1u && caps_.format_support(format) != 0u ? 1u : 0u;
}

/* ---------------------------------------------------------------- facts */

UINT D3d12Device::get_implicit_physical_adapter_mask()
{
    AV_SCOPE("D3d12Device::get_implicit_physical_adapter_mask");
    return 1u;
}

UINT D3d12Device::get_present_private_driver_data_size(const D3D12DDIARG_PRESENT_0001 *)
{
    AV_SCOPE("D3d12Device::get_present_private_driver_data_size");
    return 0u;
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

/* No pipeline is ever serialized, so no blob is this driver's. */
D3D12DDI_DRIVER_MATCHING_IDENTIFIER_STATUS
D3d12Device::check_driver_matching_identifier(D3D12DDI_SERIALIZED_DATA_TYPE,
                                              const D3D12DDI_SERIALIZED_DATA_DRIVER_MATCHING_IDENTIFIER_0054 *)
{
    AV_SCOPE("D3d12Device::check_driver_matching_identifier");
    return D3D12DDI_DRIVER_MATCHING_IDENTIFIER_UNSUPPORTED_TYPE;
}

/* No extended feature: no video, no meta commands, no render passes, no
 * shader cache of the driver's own. */
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

/* Everything is resident: a heap's memory is the host's or the arena's for
 * as long as it lives. */
HRESULT D3d12Device::make_resident(D3D12DDIARG_MAKERESIDENT_0001 *args)
{
    AV_SCOPE("D3d12Device::make_resident");
    if (args != nullptr) args->WaitMask = 0u;
    return S_OK;
}

HRESULT D3d12Device::evict(const D3D12DDIARG_EVICT *)
{
    AV_SCOPE("D3d12Device::evict");
    return S_OK;
}

/* ---------------------------------------------------------------- queues */

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
    /* The context D3D12Core signals and waits the queue's fences on. */
    D3DDDICB_CREATECONTEXTVIRTUAL context = {};
    context.NodeOrdinal = 0u;
    context.EngineAffinity = 1u;
    if (FAILED(core_->pfnCreateContextVirtualCb(runtime, &context))) {
        log_.write("rvkumd: the kernel refused a Direct3D 12 queue's context");
        return E_OUTOFMEMORY;
    }
    queue.context = context.hContext;
    if (!sent(d3d12proto::rvk_d3d12_CreateCommandQueue12(wire(), queue.header.id, queue.type, 0u, 0u),
              "CreateCommandQueue12"))
        return E_OUTOFMEMORY;
    return S_OK;
}

VOID D3d12Device::destroy_command_queue(D3D12DDI_HCOMMANDQUEUE handle)
{
    AV_SCOPE("D3d12Device::destroy_command_queue");
    QueueRecord &queue = *static_cast<QueueRecord *>(handle.pDrvPrivate);
    static_cast<void>(sent(d3d12proto::rvk_d3d12_Destroy12(wire(), queue.header.id), "Destroy12"));
    if (queue.context != nullptr && core_ != nullptr) {
        D3DDDICB_DESTROYCONTEXT destroy = {};
        destroy.hContext = queue.context;
        static_cast<void>(core_->pfnDestroyContextCb(queue.runtime, &destroy));
    }
    queue = QueueRecord{};
}

VOID D3d12Device::execute_command_lists(D3D12DDI_HCOMMANDQUEUE handle, UINT count, const D3D12DDI_HCOMMANDLIST *lists)
{
    AV_SCOPE("D3d12Device::execute_command_lists");
    constexpr UINT most_lists = 64u;
    QueueRecord &queue = *static_cast<QueueRecord *>(handle.pDrvPrivate);
    for (UINT first = 0u; lists != nullptr && first < count; first += most_lists) {
        uint32_t ids[most_lists] = {};
        const UINT batch = count - first < most_lists ? count - first : most_lists;
        for (UINT index = 0u; index < batch; index++) ids[index] = id_of(lists[first + index].pDrvPrivate);
        if (!sent(d3d12proto::rvk_d3d12_ExecuteCommandLists12(wire(), queue.header.id, batch, ids),
                  "ExecuteCommandLists12"))
            return;
    }
    /* Finished before returning (see the class comment), and the host's
     * verdict on everything sent since the last execute read with it: a
     * command it refused - a pipeline it could not compile, a view of nothing -
     * removed the device, and this is where the runtime hears of it. */
    queue.executed++;
    uint32_t removed = 0u;
    if (!sent(d3d12proto::rvk_d3d12_Finish12(wire(), &removed), "Finish12")) return;
    if (removed != 0u) lost("the host removed the device");
}

/* Native fences only: D3D12Core signals and waits a monitored fence itself,
 * on the queue's context, and this driver reports no native fences. */
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

/* ---------------------------------------------------------------- pools, recorders, lists */

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
    if (pool.made != 0u) static_cast<void>(sent(d3d12proto::rvk_d3d12_Destroy12(wire(), pool.header.id), "Destroy12"));
    pool = PoolRecord{};
}

VOID D3d12Device::reset_command_pool(D3D12DDI_HCOMMANDPOOL_0040 handle)
{
    AV_SCOPE("D3d12Device::reset_command_pool");
    const PoolRecord &pool = *static_cast<const PoolRecord *>(handle.pDrvPrivate);
    if (pool.made == 0u) return;
    static_cast<void>(sent(d3d12proto::rvk_d3d12_ResetCommandAllocator12(wire(), pool.header.id),
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
                                         D3D12DDI_HRTCOMMANDLIST)
{
    AV_SCOPE("D3d12Device::create_command_list");
    if (args == nullptr || handle.pDrvPrivate == nullptr) return E_INVALIDARG;
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
    if (list.made != 0u) static_cast<void>(sent(d3d12proto::rvk_d3d12_Destroy12(wire(), list.header.id), "Destroy12"));
    list = ListRecord{};
}

/* ---------------------------------------------------------------- fences, queries */

SIZE_T D3d12Device::calc_private_fence_size(const D3D12DDIARG_CREATE_FENCE *)
{
    AV_SCOPE("D3d12Device::calc_private_fence_size");
    return sizeof(ObjectRecord);
}

/* D3D12Core keeps the fence - a monitored fence the kernel made - and signals
 * it; the record is for a native fence's signal, which never comes. */
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
    if (!sent(d3d12proto::rvk_d3d12_CreateQueryHeap12(wire(), heap.header.id, heap.kind, args->Count),
              "CreateQueryHeap12"))
        return E_OUTOFMEMORY;
    return S_OK;
}

VOID D3d12Device::destroy_query_heap(D3D12DDI_HQUERYHEAP handle)
{
    AV_SCOPE("D3d12Device::destroy_query_heap");
    ObjectRecord &heap = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    static_cast<void>(sent(d3d12proto::rvk_d3d12_Destroy12(wire(), heap.header.id), "Destroy12"));
    heap = ObjectRecord{};
}

}  // namespace av::d3d12
