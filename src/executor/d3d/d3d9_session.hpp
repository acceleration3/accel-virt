#ifndef AV_D3D9_SESSION_HPP
#define AV_D3D9_SESSION_HPP

#include "core/trace/function_scope.hpp"
#include "executor/d3d/command_refusal.hpp"
#include "executor/d3d/d3d9_batch_progress.hpp"
#include "executor/d3d/d3d9_flip_shares.hpp"
#include "executor/d3d/d3d9_slices.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/d3d/guest_devices.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_server.h"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DResource9;
struct IDirect3DSurface9;
struct IUnknown;

namespace av {

class DriverLog;
class D3d9HostDevice;
class SharedSurfaces;
class SessionShares;
class D3d11HostDevice;
class GuestBytes;
class ImportTable;
class SliceStorages;
class WatchedBlocks;
class WireArena;
class WireReader;
class WireWriter;

class D3d9Session final : public d3d9proto::ServerDispatch {
public:
    static constexpr uint32_t max_dimension = 16384u;

    D3d9Session(const GuestBytes &guest, D3dObjectTable &objects, WatchedBlocks &blocks, SliceStorages &storages,
                DriverLog &lines);
    ~D3d9Session();
    D3d9Session(const D3d9Session &) = delete;
    D3d9Session &operator=(const D3d9Session &) = delete;

    void initialize(D3d9HostDevice *host, SharedSurfaces *surfaces, D3d11HostDevice *device11,
                    SessionShares *shares, ImportTable *imports);
    void shut_down();
    void end_batch();
    void discard_slice(uint32_t resource);

    bool               knows(uint32_t command_id) const;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply, WireArena &arena);
    bool               lost() const;
    bool               slicing() const;
    bool               polled() const;
    uint64_t           next_mark() const;
    uint64_t           marked() const;
    uint64_t           reached() const;


    bool on_CreateDevice9(uint32_t device, uint32_t flags, uint32_t &hresult) override;
    bool on_QueryCaps9(uint32_t count, const uint32_t *formats, uint32_t &hresult, uint8_t *caps,
                       uint64_t caps_capacity, uint64_t &caps_size, uint8_t *rows, uint64_t rows_capacity,
                       uint64_t &rows_size) override;
    bool on_Flush9() override;
    bool on_SelectDevice9(uint32_t device) override;
    bool on_Finish9(uint32_t &hresult) override;
    bool on_Destroy9(uint32_t object) override;

    bool on_CreateSurface9(uint32_t surface, uint32_t kind, uint32_t width, uint32_t height, uint32_t format,
                           uint32_t multisample, uint32_t quality, uint32_t flags) override;
    bool on_CreateTexture9(uint32_t texture, uint32_t type, uint32_t width, uint32_t height, uint32_t depth,
                           uint32_t levels, uint32_t usage, uint32_t format, uint32_t pool) override;
    bool on_CreateVertexBuffer9(uint32_t buffer, uint32_t length, uint32_t usage, uint32_t fvf,
                                uint32_t pool) override;
    bool on_CreateIndexBuffer9(uint32_t buffer, uint32_t length, uint32_t usage, uint32_t format,
                               uint32_t pool) override;
    bool on_WriteTexture9(uint32_t texture, uint32_t subresource, const d3d9proto::Box &box, uint32_t row_pitch,
                          uint32_t slice_pitch, uint32_t flags, const uint8_t *data, uint64_t data_size) override;
    bool on_WriteBuffer9(uint32_t buffer, uint32_t offset, uint32_t flags, const uint8_t *data,
                         uint64_t data_size) override;
    bool on_QueryBufferSlices9(uint32_t &supported) override;
    bool on_BindBufferSlice9(uint32_t buffer, uint64_t memory, uint64_t offset, uint64_t bytes) override;
    bool on_GenerateMips9(uint32_t texture, uint32_t filter) override;
    bool on_ReadSurface9(uint32_t surface, uint32_t subresource, uint32_t &hresult, uint32_t &width,
                         uint32_t &height, uint32_t &format, uint32_t &pitch, uint8_t *pixels,
                         uint64_t pixels_capacity, uint64_t &pixels_size) override;
    bool on_StretchRect9(uint32_t source, uint32_t source_subresource, const d3d9proto::Rect &source_rect,
                         uint32_t destination, uint32_t destination_subresource,
                         const d3d9proto::Rect &destination_rect, uint32_t filter) override;
    bool on_ColorFill9(uint32_t surface, uint32_t subresource, const d3d9proto::Rect &rect, uint32_t color) override;
    bool on_PresentPoint9(uint32_t surface, uint32_t subresource, uint64_t key, uint32_t point) override;

    bool on_CreateVertexShader9(uint32_t shader, const uint8_t *code, uint64_t code_size) override;
    bool on_CreatePixelShader9(uint32_t shader, const uint8_t *code, uint64_t code_size) override;
    bool on_CreateVertexDeclaration9(uint32_t declaration, uint32_t count,
                                     const d3d9proto::VertexElement *elements) override;
    bool on_SetVertexShader9(uint32_t shader) override;
    bool on_SetPixelShader9(uint32_t shader) override;
    bool on_SetVertexDeclaration9(uint32_t declaration) override;
    bool on_SetFvf9(uint32_t fvf) override;
    bool on_SetConstantsF9(uint32_t stage, uint32_t start, uint32_t value_count, const float *values) override;
    bool on_SetConstantsI9(uint32_t stage, uint32_t start, uint32_t value_count, const int32_t *values) override;
    bool on_SetConstantsB9(uint32_t stage, uint32_t start, uint32_t value_count, const uint32_t *values) override;
    bool on_SetRenderStates9(uint32_t value_count, const uint32_t *values) override;
    bool on_SetSamplerStates9(uint32_t value_count, const uint32_t *values) override;
    bool on_SetTextureStageStates9(uint32_t value_count, const uint32_t *values) override;
    bool on_SetTexture9(uint32_t sampler, uint32_t texture) override;
    bool on_SetStreamSource9(uint32_t stream, uint32_t buffer, uint32_t offset, uint32_t stride) override;
    bool on_SetStreamSourceFreq9(uint32_t stream, uint32_t setting) override;
    bool on_SetIndices9(uint32_t buffer) override;
    bool on_SetViewport9(const d3d9proto::Viewport &viewport) override;
    bool on_SetScissorRect9(const d3d9proto::Rect &rect) override;
    bool on_SetDepthStencil9(uint32_t surface, uint32_t subresource) override;
    bool on_SetTransform9(uint32_t state, uint32_t value_count, const float *matrix) override;
    bool on_SetMaterial9(const d3d9proto::Material &material) override;
    bool on_SetLight9(uint32_t index, const d3d9proto::Light &light) override;
    bool on_LightEnable9(uint32_t index, uint32_t enable) override;
    bool on_SetClipPlane9(uint32_t index, uint32_t value_count, const float *plane) override;
    bool on_DrawPrimitive9(uint32_t type, uint32_t start_vertex, uint32_t primitive_count) override;
    bool on_DrawIndexedPrimitive9(uint32_t type, int32_t base_vertex, uint32_t min_index, uint32_t vertex_count,
                                  uint32_t start_index, uint32_t primitive_count) override;
    bool on_CreateQuery9(uint32_t query, uint32_t type) override;
    bool on_IssueQuery9(uint32_t query, uint32_t flags) override;
    bool on_GetQueryData9(uint32_t query, uint32_t flags, uint32_t &hresult, uint8_t *data, uint64_t data_capacity,
                          uint64_t &data_size) override;
    bool on_SetRenderTarget9(uint32_t index, uint32_t surface, uint32_t subresource) override;
    bool on_Clear9(uint32_t rect_count, const d3d9proto::Rect *rects, uint32_t flags, uint32_t color, float z,
                   uint32_t stencil) override;

    bool invariants_hold() const;

private:
    IDirect3DDevice9 *device() const;
    IUnknown *owned(uint32_t id, D3dObjectKind kind) const;
    IDirect3DResource9 *buffer_object(uint32_t id) const;
    template <typename T>
    T                 *mine(uint32_t id, D3dObjectKind kind) const;
    IDirect3DSurface9 *surface(uint32_t id, uint32_t subresource) const;
    [[nodiscard]] bool keep(uint32_t id, D3dObjectKind kind, IUnknown *made);
    int32_t            read(IDirect3DSurface9 *surface, uint32_t &pitch, uint8_t *pixels, uint64_t capacity,
                            uint64_t &written);
    bool               write_level(uint32_t texture, uint32_t subresource, const d3d9proto::Box &box,
                                   uint32_t row_pitch, uint32_t flags, const uint8_t *data, uint64_t data_size);
    bool               write_volume(uint32_t texture, uint32_t subresource, const d3d9proto::Box &box,
                                    uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags, const uint8_t *data,
                                    uint64_t data_size);
    bool               write_surface(uint32_t id, uint32_t subresource, const d3d9proto::Box &box,
                                     uint32_t row_pitch, const uint8_t *data, uint64_t data_size);
    const uint32_t    *padded(const uint8_t *code, uint64_t code_size);

    D3d9HostDevice *host_;
    SharedSurfaces *surfaces_;
    D3d9FlipShares  flips_;
    D3dObjectTable &objects_;
    WatchedBlocks  &blocks_;
    GuestDevices<IDirect3DDevice9> devices_;
    DriverLog      &lines_;
    CommandRefusal  refusal_;
    WireArena      *arena_;
    D3d9BatchProgress progress_;
    D3d9Slices      slices_;
    bool            dirty_;
    bool            mark_refused_;
    bool            lost_;
};

template <typename T>
T *D3d9Session::mine(uint32_t id, D3dObjectKind kind) const
{
    AV_SCOPE("D3d9Session::mine");
    return static_cast<T *>(static_cast<void *>(owned(id, kind)));
}

}

#endif
