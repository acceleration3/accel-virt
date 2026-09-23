/* One Direct3D 11 device, run on the host: every slot of the runtime's device
 * table (D3DWDDM2_6DDI_DEVICEFUNCS) and the DXGI slots that present.
 *
 * The device speaks the Direct3D wire (tools/codegen/registry/d3d.toml) on the
 * process's session for its adapter (D3d11Session), as a host device of its
 * own on the session's stream (D3dDeviceChannel): the host makes the
 * resources, compiles the shaders and draws, on its GPU's shared Direct3D 11
 * device (src/device/executor/d3d_backend.hpp). The guest keeps only what the
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
#include "icd/d3d/d3d_device_channel.hpp"
#include "um/d3d/d3d11_ddi.hpp"
#include "um/d3d/d3d11_records.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::icd {
class LogSink;
}  // namespace av::icd

namespace av::d3d11 {

class D3d11HostCaps;
class D3d11Session;

class D3d11Device {
public:
    D3d11Device(D3d11Session &session, BlockSource &memory, icd::LogSink &log, const D3d11HostCaps &caps);
    ~D3d11Device();
    D3d11Device(const D3d11Device &) = delete;
    D3d11Device &operator=(const D3d11Device &) = delete;

    /* Joins the session, opening it on `adapter` (a D3DKMT adapter handle)
     * for a process's first device, makes the device on the host and keeps
     * what the runtime handed over. The HRESULT the runtime is answered with:
     * anything but S_OK leaves the device out of the session. */
    HRESULT initialize(uint32_t adapter, const D3D10DDIARG_CREATEDEVICE &create);

    /* DXGI's slots, over the table the root already published. */
    HRESULT present(DXGI_DDI_ARG_PRESENT *args);
    HRESULT present1(DXGI1_6_1_DDI_ARG_PRESENT *args);
    HRESULT blt(DXGI_DDI_ARG_BLT *args);
    HRESULT blt1(DXGI_DDI_ARG_BLT1 *args);
    HRESULT rotate_resource_identities(DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES *args);
    HRESULT resolve_shared_resource(DXGI_DDI_ARG_RESOLVESHAREDRESOURCE *args);
    HRESULT set_display_mode(DXGI_DDI_ARG_SETDISPLAYMODE *args);
    HRESULT query_resource_residency(DXGI_DDI_ARG_QUERYRESOURCERESIDENCY *args);

#include "um/d3d/generated/d3d11_device_ddi.inc"

private:
    static constexpr uint32_t stage_count = 6u;

    /* The record in a runtime handle's block. */
    ResourceRecord &record(D3D10DDI_HRESOURCE handle) const;
    ResourceRecord &record(DXGI_DDI_HRESOURCE handle) const;
    uint32_t        id_of(const void *object_block) const;
    uint32_t        next_id();
    /* An error the runtime learns of through the core layer's callback: the
     * only way a slot that returns nothing reports one. */
    void            set_error(HRESULT result);
    /* Whether the host removed this device, or the session is gone: asked
     * with a round trip after a call failed, since a command the host refuses
     * removes the device that sent it and says so only when asked. */
    bool            removed_on_host();
    /* Asks the runtime to bind every view again, after the host remade them. */
    void            rebind_state();
    d3d::D3dDeviceChannel &wire();

    /* The subresource's size and its layout in the shadow. */
    void     extent(const ResourceRecord &resource, uint32_t subresource, uint32_t &width, uint32_t &height,
                    uint32_t &depth) const;
    uint32_t row_bytes(const ResourceRecord &resource, uint32_t width) const;
    uint32_t rows(const ResourceRecord &resource, uint32_t height) const;
    size_t   subresource_bytes(const ResourceRecord &resource, uint32_t subresource) const;
    size_t   subresource_offset(const ResourceRecord &resource, uint32_t subresource) const;
    uint32_t subresource_count(const ResourceRecord &resource) const;

    /* `bytes` of data for one subresource, as UpdateSubresource1 takes it,
     * crossing through the blob arena when it is large. */
    void upload(const ResourceRecord &resource, uint32_t subresource, const D3D10_DDI_BOX *box, const void *data,
                size_t bytes, uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags);
    /* An upload larger than a piece of the arena, crossing in pieces. */
    void upload_buffer_pieces(uint32_t resource, const D3D10_DDI_BOX &region, const uint8_t *data, uint32_t copy_flags);
    void upload_row_pieces(const ResourceRecord &resource, uint32_t subresource, const D3D10_DDI_BOX &region,
                           const uint8_t *data, uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags);
    void upload_piece_of(uint32_t resource, uint32_t subresource, const D3D10_DDI_BOX *box, const void *data,
                         size_t bytes, uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags);
    /* What the upload of `box` (or the whole subresource) reads at these
     * pitches. */
    size_t upload_bytes(const ResourceRecord &resource, uint32_t subresource, const D3D10_DDI_BOX *box,
                        uint32_t row_pitch, uint32_t depth_pitch) const;
    /* A subresource read back from the host, tightly packed, into `out`. */
    bool   read_back(const ResourceRecord &resource, uint32_t subresource, uint8_t *out, size_t capacity);
    void   map(D3D10DDI_HRESOURCE handle, UINT subresource, D3D10_DDI_MAP type, UINT flags,
               D3D10DDI_MAPPED_SUBRESOURCE *mapped);
    void   unmap(D3D10DDI_HRESOURCE handle, UINT subresource);

    /* The kernel allocation a presented, scanned-out or shared resource
     * needs, and the context presents are made on. */
    HRESULT allocate(ResourceRecord &resource, const D3D11DDIARG_CREATERESOURCE &create,
                     D3D10DDI_HRTRESOURCE runtime);
    HANDLE  present_context();
    /* The D3DDDIFORMAT the kernel's descriptor names a DXGI format by. */
    uint32_t kernel_format(uint32_t format) const;
    /* The host's pixels of `resource` into its allocation, for the kernel. */
    HRESULT publish(ResourceRecord &resource, uint32_t subresource);
    HRESULT present_surface(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                            void *dxgi_context);

    void set_shader(uint32_t stage, D3D10DDI_HSHADER shader);
    void set_constant_buffers(uint32_t stage, UINT start, UINT count, const D3D10DDI_HRESOURCE *buffers,
                              const UINT *first, const UINT *counts);
    void set_shader_resources(uint32_t stage, UINT start, UINT count, const D3D10DDI_HSHADERRESOURCEVIEW *views);
    void set_samplers(uint32_t stage, UINT start, UINT count, const D3D10DDI_HSAMPLER *samplers);
    void create_shader(uint32_t stage, const UINT *code, D3D10DDI_HSHADER shader,
                       const D3D11_1DDIARG_SIGNATURE_ENTRY2 *inputs, UINT input_count,
                       const D3D11_1DDIARG_SIGNATURE_ENTRY2 *outputs, UINT output_count,
                       const D3D11_1DDIARG_SIGNATURE_ENTRY2 *patch, UINT patch_count,
                       const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT *stream_out);
    void destroy_object(uint32_t id);

    D3d11Session          &session_;
    BlockSource           &memory_;
    icd::LogSink          &log_;
    const D3d11HostCaps   &caps_;
    D3D10DDI_HRTDEVICE     runtime_;
    D3D10DDI_HRTCORELAYER  core_layer_;
    const D3DDDI_DEVICECALLBACKS               *kernel_;
    const D3D11DDI_CORELAYER_DEVICECALLBACKS   *core_;
    const DXGI_DDI_BASE_CALLBACKS              *dxgi_;
    ObjectStorage<d3d::D3dDeviceChannel> channel_;
    HANDLE    present_context_;
    uint32_t  feature_level_;
    uint64_t  shared_keys_;
};

}  // namespace av::d3d11

#endif  // AV_UM_D3D11_DEVICE_HPP
