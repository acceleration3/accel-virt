/* One Direct3D 9 device d3d9.dll made through OpenAdapter's CreateDevice: its
 * calls (D3DDDI_DEVICEFUNCS) translated into Direct3D 9 API commands on its
 * session's stream (tools/codegen/registry/d3d9.toml), which the host runs on
 * a device of its own on DXVK's d3d9.
 *
 * The runtime does a good deal itself (docs/research/d3d9/
 * reverse-engineering.md): it turns fixed function into shaders, manages the
 * managed pool through TexBlt and BufBlt from system-memory copies of its
 * own, and makes swap chains out of render targets it presents by blt. What
 * is left is resources and their locks, state, shaders, draws, queries, state
 * blocks and the present itself.
 *
 * Its definitions are split by what they touch: d3d9_device.cpp (lifetime,
 * state, shaders, queries, state blocks), d3d9_device_resources.cpp
 * (resources, locks, blts), d3d9_device_draws.cpp (draws and the user-pointer
 * streams they read), d3d9_device_present.cpp (flush, present, the kernel). */
#ifndef AV_UM_D3D9_DEVICE_HPP
#define AV_UM_D3D9_DEVICE_HPP

#include "core/support/object_pool.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/keyed_log.hpp"
#include "icd/d3d/d3d9_state_batch.hpp"
#include "icd/d3d/d3d9_state_block.hpp"
#include "icd/d3d/d3d9_state_shadow.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "icd/d3d/device_removal.hpp"
#include "icd/d3d/upload_sink.hpp"
#include "proto/generated/d3d9/d3d9_wire_types.h"
#include "um/d3d/batch_follower.hpp"
#include "um/d3d/d3d9_ddi.hpp"
#include "um/d3d/kernel_device.hpp"
#include "um/d3d/d3d9_records.hpp"
#include "um/d3d/paging_queue.hpp"
#include "um/icd/reentrant_call_lock.hpp"

#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
class TexelLayout;
}  // namespace av


namespace av::d3d {
class D3dSession;
class UmdAdapter;
}  // namespace av::d3d

namespace av::d3d9 {

class D3d9HostCaps;

class D3d9Device {
public:
    D3d9Device(d3d::D3dSession &session, BlockSource &memory, LogSink &log, const D3d9HostCaps &caps,
               d3d::UmdAdapter &adapter);
    ~D3d9Device();
    D3d9Device(const D3d9Device &) = delete;
    D3d9Device &operator=(const D3d9Device &) = delete;

    /* Joins the session, opening it on `kmt_adapter` for a process's first
     * device, makes the device on the host and keeps what the runtime handed
     * over. Anything but S_OK leaves the device out of the session. */
    HRESULT initialize(uint32_t kmt_adapter, const D3DDDIARG_CREATEDEVICE &create);
    d3d::UmdAdapter &adapter() const;
    /* One runtime call at a time (d3d::DeviceCall). */
    void lock_calls();
    void unlock_calls();

#include "um/d3d/generated/d3d9_device_ddi.inc"

private:
    /* A texture write's pieces as WriteTexture9 commands (TextureUpload):
     * the resource, the subresource and the lock's flags, of which only the
     * first piece keeps DISCARD. */
    class TextureWriter final : public d3d::UploadSink {
    public:
        TextureWriter(D3d9Device &device, uint32_t id, uint32_t index, uint32_t flags);
        [[nodiscard]] bool piece(const d3dproto::Box &box, const uint8_t *data, uint64_t bytes, uint32_t row_pitch,
                                 uint32_t slice_pitch, bool first) override;

    private:
        D3d9Device &device_;
        uint32_t    id_;
        uint32_t    index_;
        uint32_t    flags_;
    };

    static constexpr uint32_t streams = 16u;
    static constexpr uint32_t render_targets = 4u;
    /* The most words one state command carries (d3d9.toml), in whole
     * (state, value) pairs and (stage, state, value) triples. */
    static constexpr uint32_t render_piece = 512u;
    static constexpr uint32_t stage_piece = 768u;

    /* SetRenderStates9, SetSamplerStates9 or SetTextureStageStates9. */
    using StatesCommand = bool (*)(ClientTransport &, uint32_t, const uint32_t *);

    /* What the runtime bound to a stream: a buffer (a host one, or a
     * system-memory one the draw copies from), or a user pointer. */
    struct Stream {
        D3d9Resource *buffer;
        const void   *user;
        uint32_t      offset;
        uint32_t      stride;
    };
    /* What the host has bound to a stream, sent again only when it differs. */
    struct Bound {
        uint32_t id;
        uint32_t offset;
        uint32_t stride;
    };
    /* A host buffer user-pointer draws are streamed through, used front to
     * back and started again with a discard when full. */
    struct Ring {
        uint32_t id;
        uint32_t bytes;
        uint32_t at;
    };

    d3d::D3dDeviceChannel &wire();
    uint32_t               next_id();
    D3d9Resource          *resource(HANDLE handle) const;
    /* A shader's or declaration's handle is its host id. */
    uint32_t               id_of(HANDLE handle) const;
    HANDLE                 handle_of(uint32_t id) const;
    /* A command the transport refused: the device is lost, and the runtime
     * learns it on the call that returns. */
    HRESULT                lost(const char *call);
    /* `call` once per device: the runtime asks again on every frame. */
    void                   not_implemented(const char *call);
    /* With AV_D3D9_TRACE set in the process's environment, the first calls
     * that place a frame - targets made and bound, viewports, scissors,
     * presents - are logged with their numbers. */
    void                   trace(const char *call, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
    /* A round trip that took longer than a frame can spare, traced with its
     * microseconds; `started` is microseconds() when it began. */
    uint64_t               microseconds() const;
    void                   trace_wait(const char *call, uint64_t started, uint32_t detail);

    /* Pending render, sampler and stage states sent before what reads them. */
    [[nodiscard]] bool send_states();
    [[nodiscard]] bool send_pieces(StatesCommand send, uint32_t count, const uint32_t *values, uint32_t piece);
    /* While a state block records, a state goes into it instead of to the
     * device; otherwise it is the device's, and the shadow keeps it. */
    bool               recorded(const d3d::D3d9StateEntry &entry);
    /* A state block entry applied as the DDI call that set it would. */
    HRESULT            apply(const d3d::D3d9StateEntry &entry);
    HRESULT            apply_constant(const d3d::D3d9StateEntry &entry);
    HRESULT            send_viewport();
    HRESULT            send_scissor();

    /* Resources: the host object a surface command names for subresource
     * `index` - a surface resource's own surface, or a texture's level - as
     * (id, subresource); false for none. */
    bool    surface_of(const D3d9Resource &resource, uint32_t index, uint32_t &id, uint32_t &subresource) const;
    HRESULT create_host_object(D3d9Resource &resource, const D3DDDIARG_CREATERESOURCE &create);
    HRESULT create_surfaces(D3d9Resource &resource, const D3DDDIARG_CREATERESOURCE &create);
    void    release_resource(D3d9Resource &resource);
    /* Bytes one subresource takes laid out as a lock hands it out. */
    uint32_t row_pitch(const D3d9Resource &resource, const D3d9Subresource &subresource) const;
    size_t   subresource_bytes(const D3d9Resource &resource, const D3d9Subresource &subresource) const;
    /* Where a lock's area, box or range starts, from the start of memory laid
     * out `pitch` and `slice_pitch` apart. */
    size_t   lock_offset(const D3d9Resource &resource, const D3DDDIARG_LOCK &lock, uint32_t pitch,
                         uint32_t slice_pitch) const;
    [[nodiscard]] bool make_shadow(const D3d9Resource &resource, D3d9Subresource &subresource);
    /* Data into a texture's subresource at `box`, crossing in pieces of rows
     * when it is larger than a piece of the arena. */
    HRESULT write_texture(const D3d9Resource &resource, uint32_t index, const D3DDDIBOX &box, const uint8_t *data,
                          uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags);
    /* One piece of a texture write, sent; false when it could not be, after
     * saying why. */
    [[nodiscard]] bool write_piece(uint32_t id, uint32_t index, const d3d9proto::Box &box, const uint8_t *data,
                                   size_t bytes, uint32_t row_pitch, uint32_t flags);
    HRESULT write_buffer(uint32_t id, uint32_t offset, const void *data, uint32_t bytes, uint32_t flags);
    /* A subresource's pixels as the host has them, into the device's
     * read-back buffer; its pitch and rows. */
    HRESULT read_surface(const D3d9Resource &resource, uint32_t index, uint32_t &pitch, uint32_t &rows);
    [[nodiscard]] bool readback_room(size_t bytes);
    HRESULT unlock_subresource(const D3d9Resource &resource, D3d9Subresource &subresource, uint32_t index);
    /* One level of a system-memory texture blitted into the same face's
     * level `destination_level` of a host one. */
    HRESULT blt_level(const D3d9Resource &source, uint32_t source_index, const D3d9Resource &destination,
                      uint32_t destination_index, const RECT &area, POINT at);
    HRESULT blt_to_system(const D3d9Resource &source, const D3d9Resource &destination, const D3DDDIARG_BLT &blt);

    /* Draws: each stream and the index buffer bound as the host must have
     * them for one draw. A user pointer or a system-memory buffer is copied
     * into a ring for it; then every stream is bound from `first_vertex`, and
     * the draw counts its vertices from 0. */
    HRESULT prepare_streams(uint32_t first_vertex, uint32_t vertex_count, uint32_t stream0_bytes, bool &rebased);
    HRESULT prepare_indices(const void *user, uint32_t index_bytes, uint32_t first_index, uint32_t count,
                            uint32_t &start);
    HRESULT bind(uint32_t stream, const Bound &wanted);
    /* A buffer a draw reads while it is still locked: d3d9.dll keeps a
     * dynamic buffer locked across draws (MightDrawFromLocked), so what the
     * lock wrote so far crosses before the draw, not only at the unlock. */
    HRESULT send_locked(D3d9Resource &buffer);
    [[nodiscard]] bool ring_write(Ring &ring, bool index_buffer, uint32_t format, const void *data, uint32_t bytes,
                                  uint32_t alignment, uint32_t &offset);
    /* Vertices - or indices - a draw of `primitives` reads. */
    uint32_t vertices_of(D3DPRIMITIVETYPE type, uint32_t primitives) const;

    /* The kernel: the context presents go on, a buffer's allocation, and its
     * pixels read back into it for a present the kernel copies from pages. */
    HRESULT make_context();
    HRESULT allocate(D3d9Resource &resource);
    HRESULT publish(D3d9Resource &resource, uint32_t index);
    HRESULT follow_host();

    d3d::D3dSession                     &session_;
    BlockSource                         &memory_;
    LogSink                        &log_;
    KeyedLog                        lines_;
    /* Why the device is gone, said once, by the first call that found it. */
    d3d::DeviceRemoval              removal_;
    const D3d9HostCaps                  &caps_;
    d3d::UmdAdapter                     &adapter_;
    ReentrantCallLock                    lock_;
    ObjectStorage<d3d::D3dDeviceChannel> channel_;
    ObjectStorage<d3d11::PagingQueue>    paging_;
    ObjectStorage<d3d::BatchFollower>    follower_;
    ObjectPool<D3d9Resource>             resources_;
    ObjectPool<D3d9Query>                queries_;
    ObjectPool<d3d::D3d9StateBlock>      blocks_;
    d3d::D3d9StateBlock                 *recording_;
    d3d::D3d9StateBatch                  batch_;
    d3d::D3d9StateShadow                 shadow_;
    /* d3d9.dll builds its callback table on CreateDevice's stack
     * (reverse-engineering.md §3.2): the device keeps a copy, and kernel_
     * points at it once made. */
    D3DDDI_DEVICECALLBACKS               callbacks_;
    d3d::KernelDevice                    kernel_;
    HANDLE                               context_;
    D3DDDIARG_VIEWPORTINFO               viewport_;
    D3DDDIARG_ZRANGE                     z_range_;
    RECT                                 scissor_;
    bool                                 scissor_set_;
    Stream                               streams_[streams];
    Bound                                bound_[streams];
    D3d9Resource                        *indices_;
    uint32_t                             index_stride_;
    const void                          *user_indices_;
    uint32_t                             user_index_bytes_;
    uint32_t                             bound_indices_;
    Ring                                 vertex_ring_;
    Ring                                 index16_ring_;
    Ring                                 index32_ring_;
    uint8_t                             *readback_;
    size_t                               readback_bytes_;
    uint32_t                             traced_;
    /* While tracing: when the call in progress began, and the time the
     * device's calls took since the last present, and how many. */
    uint64_t                             call_began_;
    uint64_t                             busy_;
    uint32_t                             calls_;
    /* While tracing, per frame: draws by DDI (DrawPrimitive, DrawIndexed-
     * Primitive, their user-pointer twins), draws rebased onto a ring, and
     * the bytes written into buffers and textures. */
    uint32_t                             draws_[4];
    uint32_t                             rebased_;
    uint32_t                             null_shaders_;
    uint64_t                             written_;
};

}  // namespace av::d3d9

#endif  // AV_UM_D3D9_DEVICE_HPP
