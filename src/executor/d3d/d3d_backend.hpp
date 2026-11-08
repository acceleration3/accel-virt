#ifndef AV_D3D_BACKEND_HPP
#define AV_D3D_BACKEND_HPP

#include "executor/d3d/command_refusal.hpp"
#include "executor/d3d/d3d_batch_counter.hpp"
#include "executor/d3d/d3d11_batch_progress.hpp"
#include "executor/d3d/d3d12_session.hpp"
#include "executor/d3d/d3d9_session.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/d3d/guest_devices.hpp"
#include "executor/d3d/queue_drain.hpp"
#include "executor/d3d/watched_blocks.hpp"
#include "executor/guest_region.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "executor/budgeted_heap.hpp"
#include "executor/d3d/slice_storages.hpp"
#include "executor/d3d/constant_ring.hpp"
#include "executor/d3d/query_result_publisher.hpp"
#include "executor/session_backend.hpp"
#include "executor/eventfd_wake.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/wake_signal.hpp"
#include "proto/generated/d3d/d3d_dispatch_server.h"

#include <stdint.h>

struct ID3DDeviceContextState;
struct ID3D11Resource;
struct ID3D11Texture2D;
struct D3D11_TEXTURE2D_DESC;
struct ID3D11Buffer;
struct D3D11_BOX;

namespace av {

class ByteLedger;
class DxgiFormatLayout;

class DriverLog;
class OwnedFd;
class SharedImageShape;

struct D3dBackendProbe;

class D3d11HostDevice;
class D3d12HostDevice;
class D3d9HostDevice;
class ExecutorLog;
class ImportTable;
class SourceMemory;
class SessionShares;
class SharedSurfaces;

class D3dBackend final : public SessionBackend, public d3dproto::ServerDispatch {
public:
    static constexpr uint32_t max_rotation = 16u;

    explicit D3dBackend(ExecutorLog &log);

    [[nodiscard]] bool initialize(SessionShares &shares, ImportTable &imports, D3d11HostDevice *device11,
                                  D3d12HostDevice *device12 = nullptr, SharedSurfaces *surfaces = nullptr,
                                  D3d9HostDevice *device9 = nullptr);
    void use_host_ram(ByteLedger *ledger, uint64_t limit) override;
    void               shut_down() override;
    void               let_go_of_thread() override;

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool lose_device() override;
    void end_batch() override;
    bool drain(TeardownBudget &budget) override;
    bool poll() override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;
    void bind_signal(void (*signal)(void *context), void *context) override;
    [[nodiscard]] bool adopt(SourceMemory &memory, const BlobRole &role, uint64_t &handle) override;
    void abandon(const SourceMemory &memory, uint64_t handle) override;
    void unbind_arena(const SourceMemory &memory) override;
    void retire_arena(const SourceMemory &memory) override;
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
    bool on_CreateResource(uint32_t resource, const d3dproto::ResourceDesc &desc, uint64_t shared_key, uint64_t memory,
                           uint32_t row_pitch) override;
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
    bool dynamic_buffer(ID3D11Resource *resource) const;
    bool detach_buffer_slice(uint32_t resource, uint64_t offset, uint64_t bytes);
    bool marks_batches();
    void retire_slices(const SourceMemory &memory);
    void finish_d3d11();
    D3dBatchCounter::Marks issued() const;
    D3dBatchCounter::Marks completed() const;
    bool leave_slice(ID3D11Buffer *buffer, const SliceBindings::Binding &was);
    void drop_slice(uint32_t id);
    friend struct D3dBackendProbe;

    static constexpr uint32_t stage_count = 6u;

    void             describe_shared_image(uint64_t shared_key, IUnknown *made);
    ID3D11Texture2D         *shareable(const D3D11_TEXTURE2D_DESC &texture, uint32_t guest_misc);
    ID3D11Texture2D         *lent_texture(const D3D11_TEXTURE2D_DESC &texture, uint64_t memory, uint32_t row_pitch);
    void                     counted(ID3D11Resource *destination);
    bool                     update_in_pieces(ID3D11Resource *resource, uint32_t subresource, const D3D11_BOX &whole,
                                              const DxgiFormatLayout &layout, uint32_t row_pitch, uint32_t depth_pitch,
                                              uint32_t copy_flags, const uint8_t *data);
    bool                     copy_buffer_in_pieces(ID3D11Resource *to, uint32_t to_x, ID3D11Resource *from,
                                                   uint32_t from_x, uint64_t bytes, uint32_t copy_flags);
    bool                     all_of(const uint32_t *ids, uint32_t count, D3dObjectKind kind) const;
    bool                     ready() const;
    bool                     has_device() const;
    bool                     drain_queue(TeardownBudget &budget);
    [[nodiscard]] Status     remake_views(IUnknown *const *textures, uint32_t count);
    void rebind_remade();
    bool swap_remade(IUnknown **bound, uint32_t count) const;
    void release_all(IUnknown **held, uint32_t count) const;
    IUnknown                *remake_view(D3dObjectKind kind, IUnknown *view, ID3D11Resource *texture);
    D3dObjectKind            shader_kind(uint32_t stage) const;
    bool                     extent_of(ID3D11Resource *resource, uint32_t subresource, uint32_t &width,
                                       uint32_t &height, uint32_t &depth, uint32_t &format) const;
    void                     await_gpu_unlocked();
    ID3D11Resource          *staging_for(ID3D11Resource *resource, uint32_t subresource, uint32_t width,
                                         uint32_t height, uint32_t depth, uint32_t format);

    D3d11HostDevice        *device11_;
    SharedSurfaces         *surfaces_;
    SessionShares          *shares_;
    WakeSignal              wake_;
    EventfdWake             fence_wake_;
    MonotonicClock          clock_;
    bool                    admitted_;
    bool                    held_;
    BudgetedHeap                heap_;
    WatchedBlocks           blocks_;
    struct Remade {
        IUnknown *old_;
        IUnknown *made_;
    };
    GrowableArray<Remade>   remade_;
    GuestDevices<ID3DDeviceContextState> states_;
    DriverLog              &lines_;
    CommandRefusal          refusal_;
    WireArena              *arena_;
    D3dObjectTable          objects_;
    GuestBytes              guest_;
    GuestRegion             region_;
    ImportTable            *imports_;
    SliceStorages           slices_;
    D3d12Session            d3d12_;
    D3d9Session             d3d9_;
    QueryResultPublisher    published_;
    D3d11BatchProgress      progress11_;
    ConstantRing            constants_;
    uint64_t                missed_key_;
    D3dBatchCounter         batches_;
    QueueDrain              queue_drain_;
    uint64_t                submitted_;
    bool                    drained_;
    bool                    mark_refused_;
    bool                    show_refused_;
    bool                    lost_;
};

}

#endif
