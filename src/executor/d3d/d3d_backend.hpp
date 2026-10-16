/* One guest Direct3D device, run on the host's Direct3D.
 *
 * The wire is generated from tools/codegen/registry/d3d.toml: the generated
 * decoder checks that every command frames and every count is within the
 * registry's bounds, and hands the values here. What they mean is judged
 * here - an id naming an object of the wrong kind, a stage that is not one, a
 * format past the table - before DXVK sees any of it, because DXVK trusts its
 * caller and a guest is not a caller to trust.
 *
 * The session's Direct3D 11 device is its host GPU's shared one
 * (D3d11HostDevice), with the session's pipeline state in a context state
 * object of its own; every command runs holding that device's lock. */
#ifndef AV_D3D_BACKEND_HPP
#define AV_D3D_BACKEND_HPP

#include "core/log/driver_log.hpp"
#include "executor/d3d/command_refusal.hpp"
#include "executor/d3d/d3d11_batch_progress.hpp"
#include "executor/d3d/d3d12_session.hpp"
#include "executor/d3d/d3d9_session.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/d3d/guest_devices.hpp"
#include "executor/d3d/watched_blocks.hpp"
#include "executor/guest_region.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/shim/vulkan_buffer.hpp"
#include "executor/d3d/constant_ring.hpp"
#include "executor/d3d/query_result_publisher.hpp"
#include "executor/session_backend.hpp"
#include "executor/eventfd_wake.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/wake_signal.hpp"
#include "device/host/stdio_log.hpp"
#include "proto/generated/d3d/d3d_dispatch_server.h"

#include <stdint.h>

struct ID3DDeviceContextState;
struct ID3D11Resource;
struct ID3D11Texture2D;
struct D3D11_TEXTURE2D_DESC;
struct ID3D11Buffer;

namespace av {

class OwnedFd;
class SharedImageShape;

struct D3dBackendProbe;

class D3d11HostDevice;
class D3d12HostDevice;
class D3d9HostDevice;
class ImportTable;
class SourceMemory;
class SessionShares;
class SharedSurfaces;

class D3dBackend final : public SessionBackend, public d3dproto::ServerDispatch {
public:
    /* The most resources one rotation names: the wire's bound. */
    static constexpr uint32_t max_rotation = 16u;

    D3dBackend();

    /* Binds this session to what the guest shares through `shares` - bound
     * to its domain, or not, and then it shares nothing - to its GPU's
     * Direct3D 11 device and, where it has them, its Direct3D 12 device and
     * its Direct3D 9, and to the surfaces its presents publish
     * (SharedSurfaces); all must outlive it. False for a GPU with no
     * Direct3D 11 device: that is what says Direct3D is served at all. */
    [[nodiscard]] bool initialize(SessionShares &shares, ImportTable &imports, D3d11HostDevice *device11,
                                  D3d12HostDevice *device12 = nullptr, SharedSurfaces *surfaces = nullptr,
                                  D3d9HostDevice *device9 = nullptr);
    void               shut_down() override;

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    /* Lost once a command found it so, or once the GPU's Direct3D 11 device
     * is - or its Direct3D 12 device, for a session that opened one -
     * whichever session saw it first. */
    bool lost() const override;
    /* The GPU's Direct3D 11 device, which every session on it shares
     * (D3d11HostDevice::lose). */
    [[nodiscard]] bool lose_device() override;
    void end_batch() override;
    bool poll() override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;
    void bind_signal(void (*signal)(void *context), void *context) override;
    /* The session's region first, its arena; every import after is an
     * object's own memory - a Direct3D 12 heap's, the slice store - named by
     * its own address. */
    [[nodiscard]] bool adopt(const SourceMemory &memory, uint64_t &handle) override;
    void abandon(const SourceMemory &memory, uint64_t handle) override;
    /* A shared texture's memory, which other APIs import (feature 053);
     * Direct3D imports nothing, as it opens what it shares by id. */
    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                     OwnedFd &descriptor) override;
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     const SharedImageShape &shape, uint64_t into,
                                     uint64_t &wire_handle, uint64_t &detail) override;
    void               release_object(uint32_t kind, uint64_t wire_handle, uint64_t detail) override;


    bool on_CreateDevice11(uint32_t device, uint32_t feature_level, uint32_t flags, uint32_t &hresult,
                           uint32_t &level) override;
    bool on_SelectDevice11(uint32_t device) override;
    bool on_DestroyDevice11(uint32_t device) override;
    bool on_RotateResources11(uint32_t count, const uint32_t *resources) override;
    bool on_QueryCaps11(d3dproto::Caps11 &caps, uint8_t *formats, uint64_t formats_capacity,
                        uint64_t &formats_size) override;
    bool on_Flush11() override;
    bool on_ClearState11() override;
    bool on_Finish11(uint32_t &hresult) override;
    bool on_DestroyObject(uint32_t object) override;
    bool on_CreateResource(uint32_t resource, const d3dproto::ResourceDesc &desc, uint64_t shared_key) override;
    bool on_OpenResource(uint32_t resource, uint64_t shared_key, uint32_t &found,
                         d3dproto::ResourceDesc &desc) override;
    bool on_CreateShaderResourceView(uint32_t view, const d3dproto::ViewDesc &desc) override;
    bool on_CreateRenderTargetView(uint32_t view, const d3dproto::ViewDesc &desc) override;
    bool on_CreateDepthStencilView(uint32_t view, const d3dproto::ViewDesc &desc) override;
    bool on_CreateUnorderedAccessView(uint32_t view, const d3dproto::ViewDesc &desc) override;
    bool on_CreateShader(uint32_t shader, uint32_t stage, uint32_t input_count, const d3dproto::SignatureEntry *inputs,
                         uint32_t output_count, const d3dproto::SignatureEntry *outputs, uint32_t patch_count,
                         const d3dproto::SignatureEntry *patch_constants, uint32_t stream_out_count,
                         const d3dproto::StreamOutEntry *stream_out, uint32_t stride_count, const uint32_t *strides,
                         uint32_t rasterized_stream, const uint8_t *code, uint64_t code_size) override;
    bool on_CreateInputLayout(uint32_t layout, uint32_t element_count,
                              const d3dproto::InputElement *elements) override;
    bool on_CreateBlendState(uint32_t state, const d3dproto::BlendDesc &desc) override;
    bool on_CreateDepthStencilState(uint32_t state, const d3dproto::DepthStencilDesc &desc) override;
    bool on_CreateRasterizerState(uint32_t state, const d3dproto::RasterizerDesc &desc) override;
    bool on_CreateSampler(uint32_t sampler, const d3dproto::SamplerDesc &desc) override;
    bool on_CreateQuery(uint32_t query, uint32_t type, uint32_t misc_flags) override;
    bool on_SetShader(uint32_t stage, uint32_t shader) override;
    bool on_SetConstantBuffers(uint32_t stage, uint32_t start, uint32_t count,
                               const d3dproto::ConstantBufferBinding *buffers) override;
    bool on_SetShaderResources(uint32_t stage, uint32_t start, uint32_t count, const uint32_t *views) override;
    bool on_SetSamplers(uint32_t stage, uint32_t start, uint32_t count, const uint32_t *samplers) override;
    bool on_SetComputeUnorderedAccessViews(uint32_t start, uint32_t count,
                                           const d3dproto::UavBinding *views) override;
    bool on_SetRenderTargets(uint32_t rtv_count, const uint32_t *rtvs, uint32_t dsv, uint32_t uav_start,
                             uint32_t uav_count, const d3dproto::UavBinding *uavs) override;
    bool on_SetInputLayout(uint32_t layout) override;
    bool on_SetVertexBuffers(uint32_t start, uint32_t count, const d3dproto::VertexBufferBinding *buffers) override;
    bool on_SetIndexBuffer(uint32_t buffer, uint32_t format, uint32_t offset) override;
    bool on_SetTopology(uint32_t topology) override;
    bool on_SetViewports(uint32_t count, const d3dproto::Viewport *viewports) override;
    bool on_SetScissorRects(uint32_t count, const d3dproto::Rect *rects) override;
    bool on_SetBlendState(uint32_t state, const d3dproto::Float4 &factor, uint32_t sample_mask) override;
    bool on_SetDepthStencilState(uint32_t state, uint32_t stencil_ref) override;
    bool on_SetRasterizerState(uint32_t state) override;
    bool on_SetStreamOutTargets(uint32_t count, const d3dproto::StreamOutTarget *targets) override;
    bool on_SetPredication(uint32_t query, uint32_t value) override;
    bool on_SetResourceMinLod(uint32_t resource, float min_lod) override;
    bool on_Draw(uint32_t vertex_count, uint32_t start_vertex) override;
    bool on_DrawIndexed(uint32_t index_count, uint32_t start_index, int32_t base_vertex) override;
    bool on_DrawInstanced(uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex,
                          uint32_t start_instance) override;
    bool on_DrawIndexedInstanced(uint32_t index_count, uint32_t instance_count, uint32_t start_index,
                                 int32_t base_vertex, uint32_t start_instance) override;
    bool on_DrawAuto() override;
    bool on_DrawInstancedIndirect(uint32_t buffer, uint32_t offset) override;
    bool on_DrawIndexedInstancedIndirect(uint32_t buffer, uint32_t offset) override;
    bool on_Dispatch(uint32_t x, uint32_t y, uint32_t z) override;
    bool on_DispatchIndirect(uint32_t buffer, uint32_t offset) override;
    bool on_ClearRenderTargetView(uint32_t view, const d3dproto::Float4 &color) override;
    bool on_ClearDepthStencilView(uint32_t view, uint32_t flags, float depth, uint32_t stencil) override;
    bool on_ClearUnorderedAccessViewUint(uint32_t view, const d3dproto::Uint4 &values) override;
    bool on_ClearUnorderedAccessViewFloat(uint32_t view, const d3dproto::Float4 &values) override;
    bool on_ClearView(uint32_t view, const d3dproto::Float4 &color, uint32_t rect_count,
                      const d3dproto::Rect *rects) override;
    bool on_Discard(uint32_t object, uint32_t rect_count, const d3dproto::Rect *rects) override;
    bool on_CopyResource(uint32_t destination, uint32_t source) override;
    bool on_CopySubresourceRegion(uint32_t destination, uint32_t destination_subresource, uint32_t x, uint32_t y,
                                  uint32_t z, uint32_t source, uint32_t source_subresource, uint32_t has_box,
                                  const d3dproto::Box &box, uint32_t copy_flags) override;
    bool on_ResolveSubresource(uint32_t destination, uint32_t destination_subresource, uint32_t source,
                               uint32_t source_subresource, uint32_t format) override;
    bool on_GenerateMips(uint32_t view) override;
    bool on_CopyStructureCount(uint32_t destination, uint32_t offset, uint32_t source_view) override;
    bool update_dynamic_buffer(uint32_t id, ID3D11Resource *resource, uint32_t offset, const uint8_t *data, uint64_t bytes,
                               uint32_t copy_flags);
    bool on_UpdateSubresource(uint32_t resource, uint32_t subresource, uint32_t has_box, const d3dproto::Box &box,
                              uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags, const uint8_t *data,
                              uint64_t data_size) override;
    bool on_ReadSubresource(uint32_t resource, uint32_t subresource, uint32_t &hresult, uint32_t &row_pitch,
                            uint32_t &depth_pitch, uint8_t *data, uint64_t data_capacity,
                            uint64_t &data_size) override;
    bool on_PresentPoint(uint32_t resource, uint32_t subresource, uint32_t point) override;
    bool on_QueryBufferSlices11(uint32_t &supported) override;
    bool on_BindBufferSlice(uint32_t resource, uint64_t memory, uint64_t offset, uint64_t bytes) override;
    bool on_BeginQuery(uint32_t query) override;
    bool on_EndQuery(uint32_t query) override;
    bool on_BindQueryResults(uint64_t offset, uint32_t slots) override;
    bool on_GetQueryData(uint32_t query, uint32_t flags, uint32_t &ready_out, uint8_t *data, uint64_t data_capacity,
                         uint64_t &data_size) override;

    bool invariants_hold() const;

private:
    /* The slice store `store` imported for DXVK to draw from, if it can be:
     * false for a device that cannot rename a buffer to it, and for good once
     * an import failed, and for a store other than the one held. Under the
     * device's lock. */
    bool slice_storage_for(const SourceMemory &store);
    /* The slice store's import let go of, every buffer that may be on it
     * given storage of its own first, once the GPU has finished what read it.
     * Under the device's lock. */
    void release_slice_storage();
    /* Whether `resource` is a buffer of dynamic usage: the one kind a slice
     * takes. */
    bool dynamic_buffer(ID3D11Resource *resource) const;
    friend struct D3dBackendProbe;

    static constexpr uint32_t stage_count = 6u;

    /* The object `id` names, as the kind a command needs; null for none or
     * another kind. Id 0 is null too, which is what an unbinding names. */
    /* `texture` made over memory other APIs can open, when the guest's
     * `guest_misc` shares it and the host can; null otherwise. */
    void             describe_shared_image(uint64_t shared_key, IUnknown *made);
    ID3D11Texture2D         *shareable(const D3D11_TEXTURE2D_DESC &texture, uint32_t guest_misc);
    /* A copy into `destination` has run: counted when it is a shared surface. */
    void                     counted(ID3D11Resource *destination);
    /* Whether every id in `ids` names nothing (0) or an object of `kind`. */
    bool                     all_of(const uint32_t *ids, uint32_t count, D3dObjectKind kind) const;
    /* A device is selected and not removed: what a command on the immediate
     * context needs, since without one it would run on another session's
     * pipeline state, and a removed device's commands are dropped. */
    bool                     ready() const;
    /* The session made a device: what creating an object, flushing and
     * finishing need, none of which touch pipeline state. */
    bool                     has_device() const;
    /* Every view of one of `textures` remade on the next; no_memory, with
     * the views not yet remade left as they were, when one cannot be kept. */
    [[nodiscard]] Status     remake_views(IUnknown *const *textures, uint32_t count);
    void rebind_remade();
    bool swap_remade(IUnknown **bound, uint32_t count) const;
    void release_all(IUnknown **held, uint32_t count) const;
    /* A view like `view`, of `kind`, on `texture`; null when the host refuses one. */
    IUnknown                *remake_view(D3dObjectKind kind, IUnknown *view, ID3D11Resource *texture);
    /* The kind of object a shader of `stage` is; none for no stage. */
    D3dObjectKind            shader_kind(uint32_t stage) const;
    /* One subresource's size in texels (bytes for a buffer) and its format;
     * false for a subresource the resource does not have. */
    bool                     extent_of(ID3D11Resource *resource, uint32_t subresource, uint32_t &width,
                                       uint32_t &height, uint32_t &depth, uint32_t &format) const;
    /* Inside a command holding the device's lock: returns once the GPU has
     * run what the session recorded, the lock let go meanwhile. */
    void                     await_gpu_unlocked();
    /* A CPU-readable resource one subresource of `resource` can be copied
     * into, with a reference the caller owns; null when the host refuses one. */
    ID3D11Resource          *staging_for(ID3D11Resource *resource, uint32_t subresource, uint32_t width,
                                         uint32_t height, uint32_t depth, uint32_t format);

    D3d11HostDevice        *device11_;
    SharedSurfaces         *surfaces_;
    /* What this session shares and opens, in its domain; null only while the
     * session is not initialized. */
    SessionShares          *shares_;
    /* What wakes this session's worker when a device it shares is lost
     * (DeviceTenants): bound with the session's signal, and named by its
     * Direct3D 12 half too, before which it is made. */
    WakeSignal              wake_;
    /* What wakes this session's worker when the GPU reaches a mark its
     * Direct3D 11 batches or Direct3D 12 queues left. */
    EventfdWake             fence_wake_;
    /* What a wait on the GPU measures its deadline on. */
    MonotonicClock          clock_;
    /* Whether it joined its Direct3D 11 device's tenants. */
    bool                    admitted_;
    /* The device lock, taken at a batch's first command and held to its end. */
    bool                    held_;
    /* What every table of the session grows in, and how a command refused
     * for want of it is told apart (WatchedBlocks). */
    HostHeap                heap_;
    WatchedBlocks           blocks_;
    /* A view a rotation remade, the old one held until what bound it is rebound. */
    struct Remade {
        IUnknown *old_;
        IUnknown *made_;
    };
    GrowableArray<Remade>   remade_;
    /* Each device's pipeline state, by the guest's id, and the selected
     * one, which commands run on. */
    GuestDevices<ID3DDeviceContextState> states_;
    /* QEMU's stderr, where the host log is read when a guest's command fails. */
    StdioLog                sink_;
    DriverLog               lines_;
    CommandRefusal          refusal_;
    /* The decode arena of the command running, which scratch - a shader's
     * container - is taken from; null between commands. */
    WireArena              *arena_;
    D3dObjectTable          objects_;
    /* The session's blob arena, which the three dispatches resolve offsets
     * in. */
    GuestBytes              guest_;
    GuestRegion             region_;
    /* What the session imported: its region, Direct3D 12's CPU-visible
     * heaps and the slice store. Null between sessions. */
    ImportTable            *imports_;
    /* Direct3D 12's half of the session, sharing its blob arena. */
    D3d12Session            d3d12_;
    /* Direct3D 9's, sharing it too. */
    D3d9Session             d3d9_;
    QueryResultPublisher    published_;
    /* How far the host GPU has got with the batches the session recorded. */
    D3d11BatchProgress      progress11_;
    ConstantRing            constants_;
    /* The session's slice store as DXVK's to draw from, imported on the
     * first bind: what buffers bound to slices of it read (BindBufferSlice). */
    VulkanBuffer            slice_storage_;
    const SourceMemory     *slice_memory_;
    bool                    slice_storage_refused_;
    /* The last shared key an open found nothing under: the guest asks again
     * and again for one it was just given, and the host log says so once. */
    uint64_t                missed_key_;
    /* The host log says once that batches complete as recorded. */
    bool                    mark_refused_;
    /* The host log says once that a flip is read from the buffer itself. */
    bool                    show_refused_;
    bool                    lost_;
};

}  // namespace av

#endif  // AV_D3D_BACKEND_HPP
