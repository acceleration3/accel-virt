#ifndef AV_UM_D3D9_DEVICE_HPP
#define AV_UM_D3D9_DEVICE_HPP

#include "core/support/object_pool.hpp"
#include "core/trace/function_scope.hpp"
#include "core/support/object_storage.hpp"
#include "icd/common/loss_latch.hpp"
#include "icd/common/refused_calls.hpp"
#include "icd/d3d/context_slices.hpp"
#include "icd/d3d/d3d9_state_batch.hpp"
#include "icd/d3d/d3d9_state_block.hpp"
#include "icd/d3d/d3d9_state_shadow.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "icd/d3d/upload_sink.hpp"
#include "proto/escape/next_point_record.hpp"
#include "proto/generated/d3d9/d3d9_wire_types.h"
#include "um/common/performance_clock.hpp"
#include "um/d3d/present_order.hpp"
#include "um/d3d/d3d_call_trace.hpp"
#include "um/d3d/d3d9_ddi.hpp"
#include "um/d3d/kernel_device.hpp"
#include "um/d3d/d3d9_records.hpp"
#include "um/d3d/presented_allocations.hpp"
#include "icd/d3d/reentrant_call_lock.hpp"
#include "um/common/srw_lock.hpp"
#include "um/common/windows_calling_thread.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class KeyedLog;
class TexelLayout;
}


namespace av::d3d {
class D3dSession;
class UmdAdapter;
}

namespace av::d3d9 {

class D3d9HostCaps;

class D3d9Device {
public:
    D3d9Device(d3d::D3dSession &session, BlockSource &memory, KeyedLog &log, const D3d9HostCaps &caps,
               d3d::UmdAdapter &adapter, d3d::D3dCallTrace &trace);
    ~D3d9Device();
    D3d9Device(const D3d9Device &) = delete;
    D3d9Device &operator=(const D3d9Device &) = delete;

    HRESULT initialize(uint32_t kmt_adapter, const D3DDDIARG_CREATEDEVICE &create);
    d3d::UmdAdapter &adapter() const;
    void note_failure(const char *call, uint32_t hresult);
    void lock_calls();
    void unlock_calls();
    template <typename... Arguments>
    void trace_call(const char *slot, const Arguments &...arguments);

#include "um/d3d/generated/d3d9_device_ddi.inc"

private:
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
    static constexpr uint32_t render_piece = 512u;
    static constexpr uint32_t stage_piece = 768u;

    using StatesCommand = bool (*)(ClientTransport &, uint32_t, const uint32_t *);

    struct Stream {
        D3d9Resource *buffer;
        const void   *user;
        uint32_t      offset;
        uint32_t      stride;
    };
    struct Bound {
        uint32_t id;
        uint32_t offset;
        uint32_t stride;
    };
    struct Ring {
        uint32_t id;
        uint32_t bytes;
        uint32_t at;
    };

    d3d::D3dDeviceChannel &wire();
    uint32_t               next_id();
    D3d9Resource          *resource(HANDLE handle) const;
    uint32_t               id_of(HANDLE handle) const;
    HANDLE                 handle_of(uint32_t id) const;
    HRESULT                lost(const char *call);
    void                   not_implemented(const char *call);
    void                   trace(const char *call, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
    void                   trace_wait(const char *call, uint64_t started, uint32_t detail);

    [[nodiscard]] bool send_states();
    [[nodiscard]] bool send_pieces(StatesCommand send, uint32_t count, const uint32_t *values, uint32_t piece);
    bool               recorded(const d3d::D3d9StateEntry &entry);
    HRESULT            apply(const d3d::D3d9StateEntry &entry);
    HRESULT            apply_constant(const d3d::D3d9StateEntry &entry);
    HRESULT            send_viewport();
    HRESULT            send_scissor();

    bool    surface_of(const D3d9Resource &resource, uint32_t index, uint32_t &id, uint32_t &subresource) const;
    HRESULT create_host_object(D3d9Resource &resource, const D3DDDIARG_CREATERESOURCE &create);
    HRESULT create_surfaces(D3d9Resource &resource, const D3DDDIARG_CREATERESOURCE &create);
    void    release_resource(D3d9Resource &resource);
    [[nodiscard]] bool refuses_opened(const D3d9Resource &resource, const char *call);
    HRESULT            refuse_open(const D3DDDIARG_OPENRESOURCE &args, uint32_t kind);
    uint32_t row_pitch(const D3d9Resource &resource, const D3d9Subresource &subresource) const;
    size_t   subresource_bytes(const D3d9Resource &resource, const D3d9Subresource &subresource) const;
    size_t   lock_offset(const D3d9Resource &resource, const D3DDDIARG_LOCK &lock, uint32_t pitch,
                         uint32_t slice_pitch) const;
    [[nodiscard]] bool make_shadow(const D3d9Resource &resource, D3d9Subresource &subresource);
    HRESULT write_texture(const D3d9Resource &resource, uint32_t index, const D3DDDIBOX &box, const uint8_t *data,
                          uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags);
    [[nodiscard]] bool write_piece(uint32_t id, uint32_t index, const d3d9proto::Box &box, const uint8_t *data,
                                   size_t bytes, uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags);
    HRESULT write_buffer(uint32_t id, uint32_t offset, const void *data, uint32_t bytes, uint32_t flags);
    HRESULT read_surface(const D3d9Resource &resource, uint32_t index, uint32_t &pitch, uint32_t &rows);
    [[nodiscard]] bool readback_room(size_t bytes);
    HRESULT unlock_subresource(const D3d9Resource &resource, D3d9Subresource &subresource, uint32_t index);
    bool    sliceable(const D3d9Resource &resource) const;
    void    merge_nested_lock(D3d9Subresource &subresource, const D3DDDIARG_LOCK &inner);
    HRESULT lock_on_slice(D3d9Resource &resource, D3d9Subresource &subresource, const D3DDDIARG_LOCK &lock,
                          bool &served);
    [[nodiscard]] bool place_on_slice(const D3d9Resource &resource, D3d9Subresource &subresource);
    [[nodiscard]] bool rename_slice(const D3d9Resource &resource, D3d9Subresource &subresource);
    [[nodiscard]] bool leave_slice_for_copies(const D3d9Resource &resource, D3d9Subresource &subresource);
    [[nodiscard]] bool bind_slice(const D3d9Resource &resource, const D3d9Subresource &subresource);
    void    leave_slice(D3d9Subresource &subresource);
    void    trim_memory();
    HRESULT blt_level(const D3d9Resource &source, uint32_t source_index, const D3d9Resource &destination,
                      uint32_t destination_index, const RECT &area, POINT at);
    HRESULT blt_to_system(const D3d9Resource &source, const D3d9Resource &destination, const D3DDDIARG_BLT &blt);

    HRESULT prepare_streams(uint32_t first_vertex, uint32_t vertex_count, uint32_t stream0_bytes, bool &rebased);
    HRESULT prepare_indices(const void *user, uint32_t index_bytes, uint32_t first_index, uint32_t count,
                            uint32_t &start);
    HRESULT bind(uint32_t stream, const Bound &wanted);
    HRESULT send_locked(D3d9Resource &buffer);
    [[nodiscard]] bool ring_write(Ring &ring, bool index_buffer, uint32_t format, const void *data, uint32_t bytes,
                                  uint32_t alignment, uint32_t &offset);
    uint32_t vertices_of(D3DPRIMITIVETYPE type, uint32_t primitives) const;

    HRESULT allocate(D3d9Resource &resource);
    HRESULT allocate_primary(D3d9Resource &resource, const D3DDDIARG_CREATERESOURCE &create);
    HRESULT show_primary(D3d9Resource &primary, uint32_t index);
    HRESULT publish_presented(D3d9Resource &resource, uint32_t index);
    HRESULT publish(D3d9Resource &resource, uint32_t index);
    HRESULT publish_point(D3d9Resource &resource, uint32_t index);
    Status  send_point(const D3d9Resource &resource, uint32_t point, NextPointRecord::Action action);
    void    withdraw_point(D3d9Resource &resource);
    HRESULT follow_host();

    d3d::D3dSession                     &session_;
    BlockSource                         &memory_;
    KeyedLog                       &lines_;
    icd::RefusedCalls               refused_;
    icd::LossLatch                  loss_;
    const D3d9HostCaps                  &caps_;
    d3d::UmdAdapter                     &adapter_;
    WindowsCallingThread                 caller_;
    SrwLock                              call_mutex_;
    d3d::ReentrantCallLock                lock_;
    ObjectStorage<d3d::D3dDeviceChannel> channel_;
    ObjectStorage<d3d::PresentOrder>     present_order_;
    ObjectPool<D3d9Resource>             resources_;
    ObjectPool<D3d9Query>                queries_;
    ObjectPool<d3d::D3d9StateBlock>      blocks_;
    d3d::D3d9StateBlock                 *recording_;
    d3d::D3d9StateBatch                  batch_;
    d3d::D3d9StateShadow                 shadow_;
    D3DDDI_DEVICECALLBACKS               callbacks_;
    d3d::KernelDevice                    kernel_;
    PerformanceClock                     clock_;
    d3d::PresentedAllocations            allocations_;
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
    d3d::D3dCallTrace                   &trace_;
    uint32_t                             context_number_;
    uint64_t                             call_began_;
    uint64_t                             busy_;
    uint32_t                             calls_;
    uint32_t                             draws_[4];
    uint32_t                             rebased_;
    uint32_t                             null_shaders_;
    uint64_t                             written_;
    d3d::ContextSlices                   slices_;
};

template <typename... Arguments>
void D3d9Device::trace_call(const char *slot, const Arguments &...arguments)
{
    AV_SCOPE("D3d9Device::trace_call");
    trace_.call(context_number_, slot, arguments...);
}

}

#endif
