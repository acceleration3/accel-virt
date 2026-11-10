#ifndef AV_GLICD_SLICED_BUFFERS_HPP
#define AV_GLICD_SLICED_BUFFERS_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "icd/common/stream_memory.hpp"
#include "icd/opengl/deferred_slices.hpp"
#include "icd/opengl/gl_poll_clock.hpp"
#include "icd/opengl/queued_forgets.hpp"
#include "icd/opengl/slice_groups.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class DriverLog;
class Kmt;
class StateWriter;
}

namespace av::icd {
class GpuTimeline;
class HostChannel;
}

namespace av::glicd {

class SlicePin;
struct SlicedBuffersProbe;

class SlicedBuffers {
public:
    using Row = SliceGroups::Row;

    static constexpr uint64_t most_bytes = uint64_t{64} << 20u;
    static constexpr uint64_t alignment = icd::StreamMemory::smallest;
    static constexpr int64_t  whole_buffer = INT64_MIN;
    enum : uint32_t {
        gl_no_error = 0u, gl_invalid_value = 0x0501u, gl_invalid_operation = 0x0502u,
        gl_map_coherent_bit = 0x0080u, gl_dynamic_storage_bit = 0x0100u, gl_client_storage_bit = 0x0200u,
        gl_read_only = 0x88B8u, gl_write_only = 0x88B9u, gl_read_write = 0x88BAu,
        gl_buffer_access = 0x88BBu, gl_buffer_mapped = 0x88BCu, gl_buffer_access_flags = 0x911Fu,
        gl_buffer_map_length = 0x9120u, gl_buffer_map_offset = 0x9121u, gl_buffer_storage_flags = 0x8220u,
    };

    SlicedBuffers(BlockSource &memory, icd::HostChannel &channel, icd::GpuTimeline &timeline, SliceGroups &groups,
                  uint32_t group, DriverLog &lines, const void *owner);
    SlicedBuffers(const SlicedBuffers &) = delete;
    SlicedBuffers &operator=(const SlicedBuffers &) = delete;

    void grant(Kmt &kmt);
    void stop_placing();
    void release();

    bool offers(int64_t size, uint32_t flags) const;
    [[nodiscard]] Status place(ClientTransport &wire, uint32_t buffer, uint64_t size, uint32_t flags,
                               const void *data);
    void                 forget(int32_t count, const uint32_t *buffers);
    void                 send_forgets();
    void                 reclaim_deferred();
    void                 trim();

    uint8_t *view_of(const Row &row) const;
    bool     holds(uint32_t buffer) const;
    uint32_t map(uint32_t buffer, int64_t offset, int64_t length, uint32_t access, void *&pointer);
    uint32_t unmap(uint32_t buffer);
    uint32_t mark(uint32_t buffer, int64_t offset, int64_t length);
    uint32_t read(uint32_t buffer, int64_t offset, int64_t size, void *data);
    uint32_t unpack_source(uint32_t buffer, uint64_t offset, uint64_t bytes, const uint8_t *&data);
    uint32_t pack_target(uint32_t buffer, uint64_t offset, uint64_t bytes, uint8_t *&data);
    uint32_t clear_all(uint32_t buffer, uint32_t internal_format, uint32_t format, uint32_t type, const void *data);
    uint32_t clear_range(uint32_t buffer, uint32_t internal_format, int64_t offset, int64_t size, uint32_t format,
                         uint32_t type, const void *data);
    uint32_t invalidate(uint32_t buffer, int64_t offset, int64_t length);
    void    *pointer_of(uint32_t buffer) const;
    bool     mapping(uint32_t buffer, uint32_t &access, uint64_t &offset, uint64_t &bytes) const;
    bool     storage_flags(uint32_t buffer, int64_t &value) const;
    uint32_t held() const;
    uint32_t owned() const;
    uint32_t deferred() const;
    uint32_t busy() const;
    uint32_t pinned() const;
    void     pin();
    void     unpin();
    bool     wait_recorded();
    uint64_t covering() const;
    uint64_t completed() const;
    [[nodiscard]] Status enroll();
    uint32_t group() const;
    uint32_t group_root() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t stream_kinds = 2u;
    static constexpr uint32_t upload_stream = 0u;
    static constexpr uint32_t readback_stream = 1u;
    static constexpr HostMemoryRequest::Kind stream_requests_[stream_kinds] = {HostMemoryRequest::Kind::upload,
                                                                               HostMemoryRequest::Kind::readback};

    icd::StreamMemory *stream_of(uint32_t stream);
    icd::StreamMemory *made_stream(uint32_t stream);
    bool               snapshot(uint32_t buffer, Row &copy) const;
    bool               snapshot_pinned(uint32_t buffer, Row &copy, SlicePin &pin) const;
    [[nodiscard]] Status shelve_locked(const Row &row);
    [[nodiscard]] Status queue_forget_locked(uint32_t buffer);
    void                 forget_on_host(uint32_t buffer);
    uint32_t           access_error(const Row &row, int64_t offset, int64_t length, uint32_t access) const;
    uint32_t           window_error(const Row &row, uint64_t offset, uint64_t bytes) const;

    BlockSource                     &memory_;
    icd::HostChannel                &channel_;
    icd::GpuTimeline                &timeline_;
    SliceGroups                     &groups_;
    uint32_t                         group_;
    DriverLog                       &lines_;
    const void                      *owner_;
    uint64_t                         member_;
    uint32_t                         pins_;
    DeferredSlices                   deferred_;
    QueuedForgets                    forgets_;
    GlPollClock                      clock_;
    ObjectStorage<icd::StreamMemory> streams_[stream_kinds];
    bool                             granted_;
    bool                             stopped_;

    friend struct SlicedBuffersProbe;
};

}

#endif
