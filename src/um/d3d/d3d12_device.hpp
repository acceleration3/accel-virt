#ifndef AV_UM_D3D12_DEVICE_HPP
#define AV_UM_D3D12_DEVICE_HPP

#include "core/support/object_storage.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/loss_latch.hpp"
#include "icd/common/refused_calls.hpp"
#include "icd/d3d/d3d12_address_space.hpp"
#include "icd/d3d/d3d12_commitment.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "icd/d3d/pipeline_library_blob.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"
#include "um/d3d/allocation_lender.hpp"
#include "um/d3d/batch_follower.hpp"
#include "um/d3d/d3d_call_trace.hpp"
#include "um/d3d/d3d12_ddi.hpp"
#include "um/d3d/d3d12_recorder.hpp"
#include "um/d3d/d3d12_records.hpp"
#include "um/d3d/d3d12_residency.hpp"
#include "um/d3d/heap_memories.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class KeyedLog;
}

namespace av::d3d {
class D3dSession;
}

namespace av::d3d12 {

class D3d12HostCaps;

class D3d12Device {
public:
    static constexpr UINT descriptor_bytes = 32u;

    D3d12Device(BlockSource &memory, d3d::D3dSession &session, KeyedLog &log, const D3d12HostCaps &caps,
                void *const &list_table, d3d::D3dCallTrace &trace);
    ~D3d12Device();
    D3d12Device(const D3d12Device &) = delete;
    D3d12Device &operator=(const D3d12Device &) = delete;

    HRESULT initialize(uint32_t adapter, const D3D12DDIARG_CREATEDEVICE_0109 &create);

    void unsupported(const char *slot);
    HRESULT answered(const char *slot, HRESULT hresult);
    template <typename... Arguments>
    void    trace_call(const char *slot, const Arguments &...arguments);

#include "um/d3d/generated/d3d12_device_ddi.inc"

    d3d::D3dDeviceChannel    &wire();
    d3d12proto::DescriptorRef descriptor(D3D12DDI_CPU_DESCRIPTOR_HANDLE handle) const;
    d3d12proto::DescriptorRef descriptor(D3D12DDI_GPU_DESCRIPTOR_HANDLE handle) const;
    uint32_t                  id_of(const void *block) const;
    void                      lost(const char *what);
    void                      present(D3D12DDI_HCOMMANDQUEUE queue, const D3D12DDIARG_PRESENT_0001 *args,
                                      D3D12DDI_PRESENT_0051 *out, D3D12DDI_PRESENT_CONTEXTS_0051 *contexts,
                                      D3D12DDI_PRESENT_HWQUEUES_0051 *hardware_queues);

private:
    uint32_t next_id();
    void     stamp(RecordHeader &header, uint32_t tag, uint32_t id);
    bool     sent(bool sent_ok, const char *what);
    bool     follow(const QueueRecord &queue);

    d3d12proto::ResourceDesc resource_desc(const D3D12DDIARG_CREATERESOURCE_0109 &args) const;
    uint32_t                 initial_state(const D3D12DDIARG_CREATERESOURCE_0109 &args, uint32_t heap_type) const;
    HRESULT place_resource(D3D12DDI_HHEAP heap_handle, const D3D12DDIARG_CREATERESOURCE_0109 *args,
                           const D3D12DDI_CLEAR_VALUES *clear, D3D12DDI_HRESOURCE handle);
    HRESULT heap_and_resource(const D3D12DDIARG_CREATEHEAP_0001 *heap_args, D3D12DDI_HHEAP heap_handle,
                              D3D12DDI_HRTRESOURCE runtime, const D3D12DDIARG_CREATERESOURCE_0109 *args,
                              const D3D12DDI_CLEAR_VALUES *clear, D3D12DDI_HRESOURCE handle);
    HRESULT                  make_heap(const D3D12DDIARG_CREATEHEAP_0001 &args, HeapRecord &heap, bool committed);
    HRESULT                  heap_memory(const D3D12DDIARG_CREATEHEAP_0001 &args, HeapRecord &heap);
    HRESULT                  send_heap(const HeapRecord &heap, uint64_t alignment);
    HRESULT                  charge_heap(HeapRecord &heap);
    void                     uncharge_heap(HeapRecord &heap);
    void                     release_heap_memory(HeapRecord &heap);
    void                     trim_memory();
    HRESULT                  open_existing(const D3D12DDIARG_OPENHEAP_0003 &args, HeapRecord &heap,
                                           ResourceRecord &resource);
    HRESULT                  make_resource(const D3D12DDIARG_CREATERESOURCE_0109 &args, const D3D12DDI_CLEAR_VALUES *clear,
                                           HeapRecord &heap, uint64_t offset, ResourceRecord &resource);
    void                     record_resource(const d3d12proto::ResourceDesc &desc, ResourceRecord &resource);
    d3d12proto::ClearValue   clear_value(const D3D12DDI_CLEAR_VALUES *clear) const;
    uint64_t                 allocation_bytes(const d3d12proto::ResourceDesc &desc, uint64_t &alignment);
    void                     log_unsized(const d3d12proto::ResourceDesc &desc);

    HRESULT                  reserve_resource(const D3D12DDIARG_CREATERESOURCE_0109 &args,
                                              const D3D12DDI_CLEAR_VALUES *clear, ResourceRecord &resource);
    const ResourceRecord    &tiling(ResourceRecord &resource);

    void give_back(const ResourceRecord &resource);
    HRESULT                  show(ResourceRecord &resource, D3D12DDI_HRTRESOURCE runtime,
                                  const D3D12DDIARG_CREATERESOURCE_0109 &args);

    SIZE_T  shader_bytes(const D3D12DDIARG_CREATE_SHADER_0026 &args, bool tessellation) const;
    void    make_shader(uint32_t stage, const D3D12DDIARG_CREATE_SHADER_0026 &args, bool tessellation,
                        D3D12DDI_HSHADER handle);
    uint32_t copy_signature(const D3D12DDIARG_SIGNATURE_ENTRY_0012 *entries, UINT count, uint32_t set,
                            d3d12proto::SignatureEntry *out) const;
    void    graphics_state(const D3D12DDIARG_CREATE_PIPELINE_STATE_0099 &args, d3d12proto::GraphicsState &state) const;

    const ResourceRecord &viewed(D3D12DDI_HRESOURCE handle) const;
    bool texture_view(uint32_t kind, const ResourceRecord &resource, D3D12DDI_RESOURCE_DIMENSION dimension,
                      const void *members, d3d12proto::ViewDesc &view) const;

    d3d::D3dSession                      &session_;
    KeyedLog                        &lines_;
    icd::RefusedCalls                refused_;
    const D3d12HostCaps                  &caps_;
    void *const                          &list_table_;
    D3D12DDI_HRTDEVICE                    runtime_;
    const D3D12DDI_CORELAYER_DEVICECALLBACKS_0050 *core_;
    const D3DDDI_DEVICECALLBACKS                  *kernel_;
    d3d::AllocationLender                          lender_;
    HeapMemories                          memories_;
    ObjectStorage<d3d::BatchFollower>     follower_;
    ObjectStorage<D3d12Residency>         residency_;
    ObjectStorage<d3d::D3dDeviceChannel>  channel_;
    d3d::D3d12AddressSpace                addresses_;
    d3d::D3d12Commitment                  commitment_;
    D3d12Recorder                         recorder_;
    ResourceRecord                        null_resource_;
    icd::LossLatch                        loss_;
    d3d::PipelineLibraryBlob              library_blob_;
    d3d::D3dCallTrace                    &trace_;
    uint32_t                              context_number_;
};

template <typename... Arguments>
void D3d12Device::trace_call(const char *slot, const Arguments &...arguments)
{
    AV_SCOPE("D3d12Device::trace_call");
    trace_.call(context_number_, slot, arguments...);
}

}

#endif
