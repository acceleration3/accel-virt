/* One Direct3D 11 device, run on the host: every slot of the runtime's device
 * table (D3DWDDM2_6DDI_DEVICEFUNCS) and the DXGI slots that present.
 *
 * The device speaks the Direct3D wire (tools/codegen/registry/d3d.toml) on the
 * process's session for its adapter (d3d::D3dSession), as a host device of its
 * own on the session's stream (D3dDeviceChannel): the host makes the
 * resources, compiles the shaders and draws, on its GPU's shared Direct3D 11
 * device (src/executor/d3d/d3d_backend.hpp). The guest keeps only what the
 * runtime must see - a kernel allocation for a resource that is presented,
 * scanned out or shared, and a shadow in guest memory for one the CPU maps.
 *
 * Presenting reads the host's pixels back into the presented resource's
 * allocation and hands it to the kernel through DXGI's present callback; the
 * miniport scans out or blits from there.
 *
 * Written against the WDK's own DDI types, so it builds for Windows only; what
 * does not need them - the wire, the batch, the transport - is portable and
 * tested on Linux. */
#ifndef AV_UM_D3D11_DEVICE_HPP
#define AV_UM_D3D11_DEVICE_HPP

#include "core/support/object_storage.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/keyed_log.hpp"
#include "icd/common/once_latch.hpp"
#include "icd/d3d/command_recording.hpp"
#include "icd/d3d/context_slices.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "icd/d3d/upload_sink.hpp"
#include "icd/d3d/write_maps.hpp"
#include "um/d3d/reentrant_call_lock.hpp"
#include "um/common/srw_lock.hpp"
#include "um/d3d/d3d11_call_trace.hpp"
#include "um/d3d/d3d11_ddi.hpp"
#include "um/d3d/kernel_context_gate.hpp"
#include "um/d3d/kernel_device.hpp"
#include "um/d3d/d3d11_records.hpp"
#include "um/d3d/watched_shadows.hpp"
#include "um/d3d/batch_follower.hpp"
#include "um/d3d/paging_queue.hpp"

#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
}  // namespace av


namespace av::d3d {
class D3dSession;
class SharedRowsRequest;
}  // namespace av::d3d

namespace av::d3d11 {

class D3d11HostCaps;

class D3d11Device {
public:
    D3d11Device(d3d::D3dSession &session, BlockSource &memory, LogSink &log, const D3d11HostCaps &caps,
                D3d11CallTrace &trace, const d3d::SharedRowsRequest &rows);
    ~D3d11Device();
    D3d11Device(const D3d11Device &) = delete;
    D3d11Device &operator=(const D3d11Device &) = delete;

    /* Joins the session, opening it on `adapter` (a D3DKMT adapter handle)
     * for a process's first device, makes the device on the host and keeps
     * what the runtime handed over. The HRESULT the runtime is answered with:
     * anything but S_OK leaves the device out of the session. */
    HRESULT initialize(uint32_t adapter, const D3D10DDIARG_CREATEDEVICE &create);
    /* A deferred context of `parent`: its calls record commands for the
     * parent's host device into a block of its own, which a command list
     * takes, and nothing reaches the host until the list is executed. */
    HRESULT initialize_deferred(D3d11Device &parent, const D3D11DDIARG_CREATEDEFERREDCONTEXT &create);
    /* The runtime is done with this deferred context. */
    void    retire();
    /* One runtime call at a time into this object (d3d::DeviceCall). */
    void    lock_calls();
    void    unlock_calls();
    /* The runtime's call to `slot`, as the process's call trace writes it. */
    template <typename... Arguments>
    void    trace_call(const char *slot, const Arguments &...arguments);

    /* DXGI's slots, over the table the root already published. */
    HRESULT present(DXGI_DDI_ARG_PRESENT *args);
    HRESULT present1(DXGI1_6_1_DDI_ARG_PRESENT *args);
    /* DWM's DisplayCore presents its planes here, which dxgkrnl then flips
     * through the MPO3 DDI: the older entry for runtimes below DXGI DDI 4. */
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
    /* An upload's pieces as UpdateSubresource commands (TextureUpload): the
     * resource, the subresource and the copy flags, of which only the first
     * piece keeps DISCARD. */
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
    /* D3DWDDM2_0DDI's MipLevelSwizzleTransition for a layout with no swizzled
     * levels, as the runtime's own removed-device answer writes it. */
    static constexpr UINT     no_swizzle_transition = 0xFFu;

    /* The record in a runtime handle's block. */
    ResourceRecord &record(D3D10DDI_HRESOURCE handle) const;
    ResourceRecord &record(DXGI_DDI_HRESOURCE handle) const;
    uint32_t        id_of(const void *object_block) const;
    uint32_t        next_id();
    /* An error the runtime learns of through the core layer's callback: the
     * only way a slot that returns nothing reports one. */
    void            set_error(HRESULT result);
    /* E_NOTIMPL for the DDI named `call`, which the log names: a void DDI's
     * refusal otherwise reaches the application only as a device error, and a
     * program that does not check for one fails later somewhere else. */
    void            not_implemented(const char *call);
    /* The work an asynchronous read waits on is not done yet: a status the
     * application polls on, reported without the error log's write. */
    void            still_drawing();
    /* On a deferred context, a create names an object the immediate context
     * made (the runtime handle is its block): the context-local handle is a
     * copy of that block, and nothing reaches the host. False on the
     * immediate context, which makes the object. */
    bool            context_local(void *handle, const void *immediate, size_t bytes) const;
    SIZE_T          context_handle_bytes(uint32_t type) const;
    /* Whether the host removed this device, or the session is gone: asked
     * with a round trip after a call failed, since a command the host refuses
     * removes the device that sent it and says so only when asked. */
    bool            removed_on_host();
    /* Asks the runtime to bind every view again, after the host remade them. */
    d3d::D3dDeviceChannel &wire();

    /* The subresource's size and its layout in the shadow. */
    void     extent(const ResourceRecord &resource, uint32_t subresource, uint32_t &width, uint32_t &height,
                    uint32_t &depth) const;
    uint32_t row_bytes(const ResourceRecord &resource, uint32_t width) const;
    uint32_t rows(const ResourceRecord &resource, uint32_t height) const;
    size_t   subresource_bytes(const ResourceRecord &resource, uint32_t subresource) const;
    size_t   subresource_offset(const ResourceRecord &resource, uint32_t subresource) const;
    uint32_t subresource_count(const ResourceRecord &resource) const;

    [[nodiscard]] bool upload_written(const ResourceRecord &resource, uint32_t flags);
    /* A dynamic buffer the CPU writes, made on a slice of the session's slice
     * store the host draws from in place; false, nothing taken, where the
     * session has none or this is a deferred context. */
    [[nodiscard]] bool place_on_slice(ResourceRecord &resource, size_t bytes);
    /* A discarding map: a fresh slice, the old retired behind what was
     * recorded so far, and the host told. */
    [[nodiscard]] bool rename_slice(ResourceRecord &resource);
    [[nodiscard]] bool bind_slice(const ResourceRecord &resource);
    /* The slice, if any, left (ContextSlices::leave): retired behind what was
     * recorded so far, or on a deferred context only forgotten. */
    void               leave_slice(ResourceRecord &resource);
    /* A buffer on a slice moved to the copy path: its bytes in a shadow of
     * its own, the slice left (leave_slice). False, nothing changed, with no
     * memory. */
    [[nodiscard]] bool leave_slice_for_copies(ResourceRecord &resource);
    /* On a deferred context, the copy of a record given a shadow of this
     * context's own to map, any slice it named forgotten. False, nothing
     * changed, with no memory. */
    [[nodiscard]] bool own_shadow(ResourceRecord &resource);
    /* A deferred context's copy of a record, destroyed by the runtime: its
     * own shadow given back. */
    void               forget_copy(ResourceRecord &resource);
    /* `box` (or the whole subresource) of one subresource from `data` at
     * these pitches, as UpdateSubresource1 takes it, in as many pieces as one
     * command of this context carries (TextureUpload). */
    void upload(const ResourceRecord &resource, uint32_t subresource, const D3D10_DDI_BOX *box, const void *data,
                uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags);
    void publish_shared(const ResourceRecord &resource);
    size_t piece_bytes() const;
    /* One piece, sent; false when the encoder refused it. */
    [[nodiscard]] bool upload_piece_of(uint32_t resource, uint32_t subresource, const d3dproto::Box &box,
                                       const void *data, size_t bytes, uint32_t row_pitch, uint32_t depth_pitch,
                                       uint32_t copy_flags);
    /* A subresource read back from the host, tightly packed, into `out`. */
    bool   read_back(const ResourceRecord &resource, uint32_t subresource, uint8_t *out, size_t capacity);
    void   map(D3D10DDI_HRESOURCE handle, UINT subresource, D3D10_DDI_MAP type, UINT flags,
               D3D10DDI_MAPPED_SUBRESOURCE *mapped);
    void   unmap(D3D10DDI_HRESOURCE handle, UINT subresource);

    /* The kernel allocation a presented, scanned-out or shared resource
     * needs. */
    HRESULT allocate(ResourceRecord &resource, const D3D11DDIARG_CREATERESOURCE &create,
                     D3D10DDI_HRTRESOURCE runtime);
    /* The device's kernel context, made with it through the runtime so the
     * runtime knows it: presents are made on it, and Direct3D 11's fences are
     * the runtime's own, signaled on the contexts a driver made - with none, a
     * signal is refused, and a compositor waiting on the fence (DWM, taking a
     * frame from another adapter) waits for good. */
    HRESULT make_context();
    /* The host's pixels of `resource` into its allocation, for the kernel. */
    HRESULT publish(ResourceRecord &resource, uint32_t subresource);
    /* A surface GDI draws into from the guest's CPU (a window's redirection
     * surface), which no host session made: DWM opens it to compose the
     * window. It gets a host texture of its own, refreshed from the guest's
     * pages at each present wherever they changed. */
    bool open_guest_surface(ResourceRecord &resource, const D3D10DDIARG_OPENRESOURCE &args);
    void refresh_guest_surfaces();
    void refresh_bound_guest_surfaces(const D3D10DDI_HSHADERRESOURCEVIEW *views, UINT count);
    void refresh_guest_surface(ResourceRecord &resource);
    void forget_guest_surface(const ResourceRecord &resource);
    HRESULT show_flip(const ResourceRecord &source, uint32_t subresource);
    /* What a multiplane present shows of `source` is the host's frame of it as
     * it is now: in its pages for one no host texture is shared from, under
     * its key otherwise, with the batch that drew it on its way. */
    HRESULT stage_plane(ResourceRecord &source, uint32_t subresource);
    HRESULT present_surface(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                            void *dxgi_context);
    HRESULT present_pages(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                          void *dxgi_context);
    /* A blt-model present of a shared buffer the host stages in the buffer's
     * command order instead of the buffer being read back: the kernel is
     * handed the point (docs/shared-surfaces.md). */
    HRESULT present_point(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                          void *dxgi_context);
    /* `point` the present's point on its source, which the kernel moves the
     * pixels from the host's frame of; 0 for the source's pages. */
    HRESULT present_to_kernel(ResourceRecord &source, ResourceRecord *destination, void *dxgi_context,
                              uint32_t point);
    /* The kernel context's next packet - a present, a fence the runtime
     * signals - waits for the batches the ring carried: the kernel's refusal
     * otherwise, logged the first time. */
    HRESULT follow_host();
    /* A line naming what the kernel was handed: a refusal, a mode set, a
     * change in how presents are flagged. */
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
    /* The subresources mapped for writing, and how (WriteMaps). */
    d3d::WriteMaps         maps_;
    LogSink          &log_;
    KeyedLog         lines_;
    const D3d11HostCaps   &caps_;
    D3D10DDI_HRTDEVICE     runtime_;
    D3D10DDI_HRTCORELAYER  core_layer_;
    d3d::KernelDevice                           kernel_;
    const D3D11DDI_CORELAYER_DEVICECALLBACKS   *core_;
    const DXGI_DDI_BASE_CALLBACKS              *dxgi_;
    ObjectStorage<d3d::D3dDeviceChannel> channel_;
    /* The session's slices on an immediate context, none on a deferred one. */
    d3d::ContextSlices                   slices_;
    /* A deferred context's: the device it records for, and its recording,
     * which grows from its first size to its largest as a list needs. */
    static constexpr size_t recording_bytes = 4u << 20u;
    static constexpr size_t most_recording_bytes = 256u << 20u;
    D3d11Device                          *parent_;
    SrwLock                               recording_lock_;
    /* The runtime answers some calls by calling the same device back on the
     * same thread (a rotate asks it to bind its state again). */
    ReentrantCallLock                     calls_;
    ObjectStorage<d3d::D3dCommandStream>  recording_;
    ObjectStorage<d3d::CommandRecording>  recorded_;
    HANDLE    context_;
    ObjectStorage<PagingQueue> paging_;
    ObjectStorage<d3d::BatchFollower> follower_;
    /* What the session's transport waits for after this device's flip-model
     * present (KmtTransport::hold): the kernel context's queue. */
    ObjectStorage<d3d::KernelContextGate> gate_;
    uint32_t  feature_level_;
    static constexpr uint32_t max_guest_surfaces = 256u;
    ResourceRecord *guest_surfaces_[max_guest_surfaces];
    uint32_t        guest_surface_count_;
    icd::OnceLatch  guest_lock_refused_;
    icd::OnceLatch  present_refused_;
    /* The flags of the last present, ~0 before any: a fullscreen change shows
     * in the log as a change of these. */
    uint32_t        present_flags_;
    icd::OnceLatch  follow_refused_;
    icd::OnceLatch  token_refused_;
    /* Presents so far: a GDI surface is refreshed once a frame, when first bound. */
    uint32_t        frame_;
    D3d11CallTrace &trace_;
    /* Which threads make shared textures another API reads as rows. */
    const d3d::SharedRowsRequest &rows_;
    /* What the call trace calls this device or deferred context. */
    uint32_t        context_number_;
};

template <typename... Arguments>
void D3d11Device::trace_call(const char *slot, const Arguments &...arguments)
{
    AV_SCOPE("D3d11Device::trace_call");
    trace_.call(context_number_, slot, arguments...);
}

}  // namespace av::d3d11

#endif  // AV_UM_D3D11_DEVICE_HPP
