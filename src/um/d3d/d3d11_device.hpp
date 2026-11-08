#ifndef AV_UM_D3D11_DEVICE_HPP
#define AV_UM_D3D11_DEVICE_HPP

#include "core/support/growable_array.hpp"
#include "core/support/object_storage.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/loss_latch.hpp"
#include "icd/common/refused_calls.hpp"
#include "icd/common/once_latch.hpp"
#include "icd/common/open_mappings.hpp"
#include "icd/common/staged_mapping.hpp"
#include "icd/common/written_runs.hpp"
#include "icd/d3d/command_recording.hpp"
#include "icd/d3d/context_slices.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "icd/d3d/upload_sink.hpp"
#include "icd/d3d/reentrant_call_lock.hpp"
#include "um/common/windows_calling_thread.hpp"
#include "um/common/srw_lock.hpp"
#include "um/d3d/d3d_call_trace.hpp"
#include "um/d3d/d3d11_ddi.hpp"
#include "um/d3d/present_order.hpp"
#include "um/d3d/allocation_lender.hpp"
#include "um/d3d/cross_adapter_waits.hpp"
#include "um/d3d/kernel_device.hpp"
#include "um/d3d/d3d11_records.hpp"
#include "um/d3d/watched_shadows.hpp"
#include "um/common/performance_clock.hpp"
#include "um/d3d/presented_allocations.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class KeyedLog;
}


namespace av::d3d {
class D3dSession;
class SharedRowsRequest;
}

namespace av::d3d11 {

class D3d11HostCaps;

class D3d11Device {
public:
    D3d11Device(d3d::D3dSession &session, BlockSource &memory, KeyedLog &log, const D3d11HostCaps &caps,
                d3d::D3dCallTrace &trace, const d3d::SharedRowsRequest &rows);
    ~D3d11Device();
    D3d11Device(const D3d11Device &) = delete;
    D3d11Device &operator=(const D3d11Device &) = delete;

    HRESULT initialize(uint32_t adapter, const D3D10DDIARG_CREATEDEVICE &create);
    HRESULT initialize_deferred(D3d11Device &parent, const D3D11DDIARG_CREATEDEFERREDCONTEXT &create);
    void    retire();
    void    lock_calls();
    void    unlock_calls();
    template <typename... Arguments>
    void    trace_call(const char *slot, const Arguments &...arguments);

    HRESULT present(DXGI_DDI_ARG_PRESENT *args);
    HRESULT present1(DXGI1_6_1_DDI_ARG_PRESENT *args);
    HRESULT present_multiplane_overlay(DXGI_DDI_ARG_PRESENTMULTIPLANEOVERLAY *args);
    HRESULT present_multiplane_overlay1(DXGI1_6_1_DDI_ARG_PRESENTMULTIPLANEOVERLAY *args);
    HRESULT blt(DXGI_DDI_ARG_BLT *args);
    HRESULT blt1(DXGI_DDI_ARG_BLT1 *args);
    HRESULT rotate_resource_identities(DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES *args);
    HRESULT resolve_shared_resource(DXGI_DDI_ARG_RESOLVESHAREDRESOURCE *args);
    HRESULT set_display_mode(DXGI_DDI_ARG_SETDISPLAYMODE *args);
    HRESULT query_resource_residency(DXGI_DDI_ARG_QUERYRESOURCERESIDENCY *args);

#include "um/d3d/generated/d3d11_device_ddi.inc"

private:
    class UpdateWriter final : public d3d::UploadSink {
    public:
        UpdateWriter(D3d11Device &device, uint32_t resource, uint32_t subresource, uint32_t flags);
        [[nodiscard]] bool piece(const d3dproto::Box &box, const uint8_t *data, uint64_t bytes, uint32_t row_pitch,
                                 uint32_t slice_pitch, bool first) override;

    private:
        D3d11Device &device_;
        uint32_t     resource_;
        uint32_t     subresource_;
        uint32_t     flags_;
    };

    static constexpr uint32_t stage_count = 6u;
    static constexpr UINT     no_swizzle_transition = 0xFFu;

    ResourceRecord &record(D3D10DDI_HRESOURCE handle) const;
    ResourceRecord &record(DXGI_DDI_HRESOURCE handle) const;
    uint32_t        id_of(const void *object_block) const;
    uint32_t        next_id();
    void            set_error(HRESULT result);
    HRESULT         lost(const char *entry);
    void            not_implemented(const char *call);
    void            still_drawing();
    bool            staging_pending(ResourceRecord &resource);
    void            note_gpu_write(D3D10DDI_HRESOURCE handle);
    bool            context_local(void *handle, const void *immediate, size_t bytes) const;
    SIZE_T          context_handle_bytes(uint32_t type) const;
    bool            removed_on_host();
    d3d::D3dDeviceChannel &wire();

    void     extent(const ResourceRecord &resource, uint32_t subresource, uint32_t &width, uint32_t &height,
                    uint32_t &depth) const;
    uint32_t row_bytes(const ResourceRecord &resource, uint32_t width) const;
    uint32_t rows(const ResourceRecord &resource, uint32_t height) const;
    size_t   subresource_bytes(const ResourceRecord &resource, uint32_t subresource) const;
    size_t   subresource_offset(const ResourceRecord &resource, uint32_t subresource) const;
    uint32_t subresource_count(const ResourceRecord &resource) const;

    [[nodiscard]] bool upload_written(const ResourceRecord &resource, uint32_t flags);
    [[nodiscard]] bool open_map(const ResourceRecord &resource, uint32_t subresource, size_t offset, size_t bytes,
                                uint32_t flags);
    void               forget_maps(uint32_t id);
    [[nodiscard]] bool place_on_slice(ResourceRecord &resource, size_t bytes);
    [[nodiscard]] bool rename_slice(ResourceRecord &resource);
    [[nodiscard]] bool bind_slice(const ResourceRecord &resource);
    [[nodiscard]] bool detach_slice(const ResourceRecord &resource);
    void               leave_slice(ResourceRecord &resource);
    [[nodiscard]] bool leave_slice_for_copies(ResourceRecord &resource);
    [[nodiscard]] bool leave_immediate_slice(const ResourceRecord &resource);
    [[nodiscard]] bool own_shadow(ResourceRecord &resource);
    void               forget_copy(ResourceRecord &resource);
    void upload(const ResourceRecord &resource, uint32_t subresource, const D3D10_DDI_BOX *box, const void *data,
                uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags);
    void publish_shared(const ResourceRecord &resource);
    size_t piece_bytes() const;
    [[nodiscard]] bool upload_piece_of(uint32_t resource, uint32_t subresource, const d3dproto::Box &box,
                                       const void *data, size_t bytes, uint32_t row_pitch, uint32_t depth_pitch,
                                       uint32_t copy_flags);
    bool   read_back(const ResourceRecord &resource, uint32_t subresource, uint8_t *out, size_t capacity);
    void   map(D3D10DDI_HRESOURCE handle, UINT subresource, D3D10_DDI_MAP type, UINT flags,
               D3D10DDI_MAPPED_SUBRESOURCE *mapped);
    void   unmap(D3D10DDI_HRESOURCE handle, UINT subresource);

    HRESULT allocate(ResourceRecord &resource, const D3D11DDIARG_CREATERESOURCE &create,
                     D3D10DDI_HRTRESOURCE runtime);
    HRESULT publish(ResourceRecord &resource, uint32_t subresource);
    bool open_guest_surface(ResourceRecord &resource, const D3D10DDIARG_OPENRESOURCE &args);
    bool is_cross_adapter(const D3D11DDIARG_CREATERESOURCE &args, const ResourceRecord &resource) const;
    bool open_cross_adapter(ResourceRecord &resource, const D3D10DDIARG_OPENRESOURCE &args);
    void refresh_guest_surfaces();
    void refresh_bound_guest_surfaces(const D3D10DDI_HSHADERRESOURCEVIEW *views, UINT count);
    void refresh_guest_surface(ResourceRecord &resource);
    bool begin_pages_read(const ResourceRecord &resource, uint64_t &writes);
    uint64_t end_pages_read(const ResourceRecord &resource, uint64_t begun);
    void forget_guest_surface(const ResourceRecord &resource);
    HRESULT show_flip(const ResourceRecord &source, uint32_t subresource);
    HRESULT stage_plane(ResourceRecord &source, uint32_t subresource);
    template <typename Plane>
    HRESULT stage_planes(const Plane *planes, UINT count, D3DKMT_HANDLE *allocations, bool &flipped);
    HRESULT present_surface(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                            void *dxgi_context);
    HRESULT present_pages(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                          void *dxgi_context);
    HRESULT present_point(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                          void *dxgi_context);
    HRESULT present_to_kernel(ResourceRecord &source, ResourceRecord *destination, void *dxgi_context,
                              uint32_t point);
    HRESULT follow_host();
    void    log_present(const char *what, uint32_t value, const ResourceRecord &source);

    void set_shader(uint32_t stage, D3D10DDI_HSHADER shader);
    void set_constant_buffers(uint32_t stage, UINT start, UINT count, const D3D10DDI_HRESOURCE *buffers,
                              const UINT *first, const UINT *counts);
    void set_shader_resources(uint32_t stage, UINT start, UINT count, const D3D10DDI_HSHADERRESOURCEVIEW *views);
    void set_samplers(uint32_t stage, UINT start, UINT count, const D3D10DDI_HSAMPLER *samplers);
    void stream_vertex_output(const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT &args, D3D10DDI_HSHADER shader,
                              const D3D11_1DDIARG_STAGE_IO_SIGNATURES &signatures);
    void create_shader(uint32_t stage, const UINT *code, D3D10DDI_HSHADER shader,
                       const D3D11_1DDIARG_SIGNATURE_ENTRY2 *inputs, UINT input_count,
                       const D3D11_1DDIARG_SIGNATURE_ENTRY2 *outputs, UINT output_count,
                       const D3D11_1DDIARG_SIGNATURE_ENTRY2 *patch, UINT patch_count,
                       const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT *stream_out);
    void destroy_object(uint32_t id);

    d3d::D3dSession          &session_;
    BlockSource           &memory_;
    WatchedShadows         shadows_;
    icd::OpenMappings<icd::StagedMapping> maps_;
    icd::WrittenRuns       written_;
    KeyedLog        &lines_;
    icd::RefusedCalls refused_;
    icd::LossLatch    loss_;
    const D3d11HostCaps   &caps_;
    D3D10DDI_HRTDEVICE     runtime_;
    D3D10DDI_HRTCORELAYER  core_layer_;
    d3d::KernelDevice                           kernel_;
    d3d::AllocationLender                       lender_;
    PerformanceClock                            clock_;
    d3d::PresentedAllocations                   allocations_;
    const D3D11DDI_CORELAYER_DEVICECALLBACKS   *core_;
    const DXGI_DDI_BASE_CALLBACKS              *dxgi_;
    ObjectStorage<d3d::D3dDeviceChannel> channel_;
    d3d::ContextSlices                   slices_;
    static constexpr size_t recording_bytes = 4u << 20u;
    static constexpr size_t most_recording_bytes = 256u << 20u;
    D3d11Device                          *parent_;
    SrwLock                               recording_lock_;
    WindowsCallingThread                  caller_;
    SrwLock                               call_mutex_;
    d3d::ReentrantCallLock                 calls_;
    ObjectStorage<d3d::D3dCommandStream>  recording_;
    ObjectStorage<d3d::CommandRecording>  recorded_;
    HANDLE    context_;
    ObjectStorage<d3d::PresentOrder> present_order_;
    ObjectStorage<d3d::CrossAdapterWaits> waits_;
    uint32_t  feature_level_;
    GrowableArray<ResourceRecord *> guest_surfaces_;
    icd::OnceLatch  guest_lock_refused_;
    icd::OnceLatch  pages_read_refused_;
    icd::OnceLatch  pages_read_late_;
    icd::OnceLatch  present_refused_;
    uint32_t        present_flags_;
    icd::OnceLatch  follow_refused_;
    icd::OnceLatch  token_refused_;
    uint32_t        frame_;
    d3d::D3dCallTrace &trace_;
    const d3d::SharedRowsRequest &rows_;
    uint32_t        context_number_;
};

template <typename... Arguments>
void D3d11Device::trace_call(const char *slot, const Arguments &...arguments)
{
    AV_SCOPE("D3d11Device::trace_call");
    trace_.call(context_number_, slot, arguments...);
}

}

#endif
