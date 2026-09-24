/* One Direct3D 12 device, run on the host: every slot of the device table
 * D3D12Core asks for (D3D12DDI_DEVICE_FUNCS_CORE_0109) and the command queue's
 * (D3D12DDI_COMMAND_QUEUE_FUNCS_CORE_0001).
 *
 * The device speaks the Direct3D 12 wire (tools/codegen/registry/d3d12.toml)
 * on its process's Direct3D 12 session for the adapter (D3dSession), and the
 * host makes every object on its GPU's vkd3d-proton device
 * (src/device/executor/d3d12_session.hpp). The guest keeps what the runtime
 * must be able to ask without a round trip: the records in the runtime's
 * blocks (d3d12_records.hpp), the GPU virtual addresses it hands out
 * (D3d12AddressSpace), and the pages of upload and readback heaps, which are
 * the session arena's (D3d12HeapArena) and which the host imports.
 *
 * A queue's work is finished before ExecuteCommandLists returns. D3D12Core
 * signals an application's fences itself, on the kernel context the driver
 * made for the queue, as soon as that context's earlier submissions are done
 * - and the work never went through that context, it went through the ring.
 * Waiting for the host is what keeps a fence from reaching its value early.
 *
 * Written against the WDK's own DDI types, so it builds for Windows only. */
#ifndef AV_UM_D3D12_DEVICE_HPP
#define AV_UM_D3D12_DEVICE_HPP

#include "core/support/object_storage.hpp"
#include "icd/d3d/d3d12_address_space.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"
#include "um/d3d/d3d12_ddi.hpp"
#include "um/d3d/d3d12_recorder.hpp"
#include "um/d3d/d3d12_records.hpp"

#include <stdint.h>

namespace av::icd {
class LogSink;
}  // namespace av::icd

namespace av::d3d {
class D3dSession;
class D3d12HeapArena;
}  // namespace av::d3d

namespace av::d3d12 {

class D3d12HostCaps;

class D3d12Device {
public:
    /* Each descriptor's slot in a descriptor heap's block: room for its
     * DescriptorSlot, and what GetDescriptorSizeInBytes answers. */
    static constexpr UINT descriptor_bytes = 32u;

    D3d12Device(d3d::D3dSession &session, d3d::D3d12HeapArena &heaps, icd::LogSink &log, const D3d12HostCaps &caps);
    ~D3d12Device();
    D3d12Device(const D3d12Device &) = delete;
    D3d12Device &operator=(const D3d12Device &) = delete;

    /* Joins the session, opening it on `adapter` (a D3DKMT adapter handle) for
     * a process's first device, and opens the host's device. The HRESULT the
     * runtime is answered with: anything but S_OK leaves the device out of the
     * session. */
    HRESULT initialize(uint32_t adapter, const D3D12DDIARG_CREATEDEVICE_0109 &create);

    /* A slot this driver has no implementation of was called: logged once. */
    void unsupported(const char *slot);

#include "um/d3d/generated/d3d12_device_ddi.inc"

    /* What the recorder records through. */
    d3d::D3dDeviceChannel    &wire();
    /* The heap and index a descriptor handle names. */
    d3d12proto::DescriptorRef descriptor(D3D12DDI_CPU_DESCRIPTOR_HANDLE handle) const;
    d3d12proto::DescriptorRef descriptor(D3D12DDI_GPU_DESCRIPTOR_HANDLE handle) const;
    /* The id in a record the runtime handed back, 0 for none. */
    uint32_t                  id_of(const void *block) const;
    /* A failure the runtime learns of through the core layer's callback: the
     * only way a slot that returns nothing reports one. Logged once. */
    void                      lost(const char *what);

private:
    uint32_t next_id();
    void     stamp(RecordHeader &header, uint32_t magic, uint32_t id);
    bool     sent(bool sent_ok, const char *what);

    /* Resources and heaps (d3d12_device_resources.cpp). */
    d3d12proto::ResourceDesc resource_desc(const D3D12DDIARG_CREATERESOURCE_0109 &args) const;
    uint32_t                 initial_state(const D3D12DDIARG_CREATERESOURCE_0109 &args, uint32_t heap_type) const;
    HRESULT                  make_heap(const D3D12DDIARG_CREATEHEAP_0001 &args, HeapRecord &heap, bool committed);
    HRESULT                  make_resource(const D3D12DDIARG_CREATERESOURCE_0109 &args, const D3D12DDI_CLEAR_VALUES *clear,
                                           HeapRecord &heap, uint64_t offset, ResourceRecord &resource);
    uint64_t                 allocation_bytes(const d3d12proto::ResourceDesc &desc, uint64_t &alignment);

    /* Shaders and pipelines (d3d12_device_pipelines.cpp). */
    SIZE_T  shader_bytes(const D3D12DDIARG_CREATE_SHADER_0026 &args, bool tessellation) const;
    void    make_shader(uint32_t stage, const D3D12DDIARG_CREATE_SHADER_0026 &args, bool tessellation,
                        D3D12DDI_HSHADER handle);
    uint32_t copy_signature(const D3D12DDIARG_SIGNATURE_ENTRY_0012 *entries, UINT count, uint32_t set,
                            d3d12proto::SignatureEntry *out) const;
    void    graphics_state(const D3D12DDIARG_CREATE_PIPELINE_STATE_0099 &args, d3d12proto::GraphicsState &state) const;

    /* Views (d3d12_device_descriptors.cpp). */
    const ResourceRecord &viewed(D3D12DDI_HRESOURCE handle) const;
    bool texture_view(uint32_t kind, const ResourceRecord &resource, D3D12DDI_RESOURCE_DIMENSION dimension,
                      const void *members, d3d12proto::ViewDesc &view) const;

    d3d::D3dSession                      &session_;
    d3d::D3d12HeapArena                  &heaps_;
    icd::LogSink                         &log_;
    const D3d12HostCaps                  &caps_;
    D3D12DDI_HRTDEVICE                    runtime_;
    const D3D12DDI_CORELAYER_DEVICECALLBACKS_0050 *core_;
    ObjectStorage<d3d::D3dDeviceChannel>  channel_;
    d3d::D3d12AddressSpace                addresses_;
    D3d12Recorder                         recorder_;
    ResourceRecord                        null_resource_;
    bool                                  lost_;
    /* Slots already logged as unsupported, by the first byte of their name
     * hashed with its length: a log line each, not one per call. */
    uint64_t                              logged_[4];
};

}  // namespace av::d3d12

#endif  // AV_UM_D3D12_DEVICE_HPP
