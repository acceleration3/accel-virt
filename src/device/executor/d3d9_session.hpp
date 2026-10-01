/* One session's Direct3D 9, run on the host's DXVK d3d9.
 *
 * The Direct3D 9 half of a Direct3D session (D3dBackend routes the commands of
 * d3d9.toml's range here): the session's devices, each a host device of its
 * own (D3d9HostDevice::make_device), the one selected, and every object they
 * made by the id the guest named it with. The wire is generated from
 * tools/codegen/registry/d3d9.toml; what the values mean - an id of the wrong
 * kind, another device's object, a size no device makes - is judged here
 * before DXVK sees it, because DXVK trusts its caller and a guest is not a
 * caller to trust.
 *
 * A refused asynchronous command does nothing and removes the selected
 * device: Finish9 answers D3DERR_DRIVERINTERNALERROR for it from then on, and
 * the session's other devices go on. A refused synchronous command has no
 * answer to give the guest waiting on it, and fails the session.
 *
 * Its handlers are in three files by what they touch: this one's .cpp (devices,
 * adapter questions, lifetime), _resources.cpp (surfaces, textures, buffers
 * and what is written into and copied between them) and _pipeline.cpp
 * (shaders, state, draws and queries). */
#ifndef AV_D3D9_SESSION_HPP
#define AV_D3D9_SESSION_HPP

#include "core/support/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "device/executor/command_refusal.hpp"
#include "device/executor/d3d_object_table.hpp"
#include "device/executor/guest_devices.hpp"
#include "device/host/stdio_log.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_server.h"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DSurface9;
struct IUnknown;

namespace av {

class D3d9HostDevice;
class SharedSurfaces;
class GuestRegion;
class WireArena;
class WireReader;
class WireWriter;

class D3d9Session final : public d3d9proto::ServerDispatch {
public:
    /* Devices one process keeps at once; a create past this is answered with
     * an error rather than made. */
    /* d3d9.dll makes a device for each DirectDraw object it opens - to
     * enumerate, to validate a create, to create, to ask multisample levels -
     * and a process may hold several at once. */
    static constexpr uint32_t max_devices = 16u;
    /* The largest surface edge any Direct3D 9 device makes. */
    static constexpr uint32_t max_dimension = 16384u;

    /* `region` is the session's blob arena, which D3dBackend adopts, and
     * `objects` the session's object table: the guest names every object of
     * a session from one id space, so Direct3D 9's share Direct3D 11's table
     * (and a table is a megabyte). */
    D3d9Session(GuestRegion &region, D3dObjectTable &objects);
    ~D3d9Session();
    D3d9Session(const D3d9Session &) = delete;
    D3d9Session &operator=(const D3d9Session &) = delete;

    /* Binds the session to its GPU's Direct3D 9, which must outlive it; null
     * is a GPU with none, on which every device create fails. */
    /* `surfaces` is where a present point's frame is kept for the present
     * engine; null where the device has none, and a present point stages
     * nothing. */
    void initialize(D3d9HostDevice *host, SharedSurfaces *surfaces);
    /* Every device and object let go. */
    void shut_down();

    bool               knows(uint32_t command_id) const;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply, WireArena &arena);

    const uint8_t *guest_bytes(uint64_t offset, uint64_t size) override;
    uint8_t       *guest_bytes_out(uint64_t offset, uint64_t size) override;

    bool on_CreateDevice9(uint32_t device, uint32_t flags, uint32_t &hresult) override;
    bool on_QueryCaps9(uint32_t &hresult, uint32_t &vendor_id, uint32_t &device_id, uint32_t &subsystem_id,
                       uint32_t &revision, uint32_t &video_memory_mb, uint8_t *caps, uint64_t caps_capacity,
                       uint64_t &caps_size) override;
    bool on_QueryFormats9(uint32_t count, const uint32_t *formats, uint8_t *support, uint64_t support_capacity,
                          uint64_t &support_size) override;
    bool on_QueryMultisample9(uint32_t format, uint32_t multisample, uint32_t &hresult,
                              uint32_t &quality_levels) override;
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
    /* The selected device, when its commands may run: null with none
     * selected, or once it is removed, which refuses them until the guest
     * makes another. */
    IDirect3DDevice9 *device() const;
    /* The selected device's object `id` if it is of `kind`; null for another
     * kind, another device's, or none. No reference is taken. */
    IUnknown *owned(uint32_t id, D3dObjectKind kind) const;
    /* The same, as the interface `kind` is kept as. */
    template <typename T>
    T                 *mine(uint32_t id, D3dObjectKind kind) const;
    /* `id` as a surface of the selected device's to draw into, read or copy:
     * the surface itself, or subresource `subresource` of a 2D texture (its
     * level) or a cube (face * levels + level); with a reference the caller
     * owns, null for anything else. */
    IDirect3DSurface9 *surface(uint32_t id, uint32_t subresource) const;
    /* `made` kept under `id`; false, with it released, where the id is taken. */
    [[nodiscard]] bool keep(uint32_t id, D3dObjectKind kind, IUnknown *made);
    /* Reads `surface` into `pixels` as ReadSurface9 answers, from memory the
     * CPU can lock: a render target goes through a system-memory copy. */
    int32_t            read(IDirect3DSurface9 *surface, uint32_t &pitch, uint8_t *pixels, uint64_t capacity,
                            uint64_t &written);
    /* WriteTexture9's copy into one 2D or cube level, and into a volume's. */
    bool               write_level(uint32_t texture, uint32_t subresource, const d3d9proto::Box &box,
                                   uint32_t row_pitch, uint32_t flags, const uint8_t *data, uint64_t data_size);
    bool               write_volume(uint32_t texture, uint32_t subresource, const d3d9proto::Box &box,
                                    uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags, const uint8_t *data,
                                    uint64_t data_size);
    /* A shader's tokens copied out of the arena with end tokens past them, for
     * DXVK to read; null where the arena has no room. */
    const uint32_t    *padded(const uint8_t *code, uint64_t code_size);

    GuestRegion    &region_;
    D3d9HostDevice *host_;
    SharedSurfaces *surfaces_;
    D3dObjectTable &objects_;
    GuestDevices<IDirect3DDevice9, max_devices> devices_;
    /* QEMU's stderr, where the host log is read when a guest's command fails. */
    StdioLog        sink_;
    DriverLog       lines_;
    CommandRefusal  refusal_;
    /* The decode arena of the command running; null between commands. */
    WireArena      *arena_;
};

template <typename T>
T *D3d9Session::mine(uint32_t id, D3dObjectKind kind) const
{
    AV_SCOPE("D3d9Session::mine");
    return static_cast<T *>(static_cast<void *>(owned(id, kind)));
}

}  // namespace av

#endif  // AV_D3D9_SESSION_HPP
