#ifndef AV_GLICD_SLICED_BUFFERS_HPP
#define AV_GLICD_SLICED_BUFFERS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "icd/common/stream_memory.hpp"
#include "icd/opengl/gl_poll_clock.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class DriverLog;
class Kmt;
class StateWriter;
class ThreadLock;
}

namespace av::icd {
class GpuTimeline;
class HostChannel;
}

namespace av::glicd {

struct SlicedBuffersProbe;

class SlicedBuffers {
public:
    static constexpr uint64_t most_bytes = uint64_t{64} << 20u;
    static constexpr uint64_t alignment = icd::StreamMemory::smallest;
    static constexpr int64_t  whole_buffer = -1;
    enum : uint32_t {
        gl_no_error = 0u, gl_invalid_value = 0x0501u, gl_invalid_operation = 0x0502u,
        gl_map_coherent_bit = 0x0080u, gl_dynamic_storage_bit = 0x0100u,
        gl_read_only = 0x88B8u, gl_write_only = 0x88B9u, gl_read_write = 0x88BAu,
        gl_buffer_access = 0x88BBu, gl_buffer_mapped = 0x88BCu, gl_buffer_access_flags = 0x911Fu,
        gl_buffer_map_length = 0x9120u, gl_buffer_map_offset = 0x9121u, gl_buffer_storage_flags = 0x8220u,
    };

    SlicedBuffers(BlockSource &memory, icd::HostChannel &channel, icd::GpuTimeline &timeline, ThreadLock &lock,
                  DriverLog &lines, const void *owner);
    SlicedBuffers(const SlicedBuffers &) = delete;
    SlicedBuffers &operator=(const SlicedBuffers &) = delete;

    void grant(Kmt &kmt);
    void stop_placing();
    void release();

    bool offers(uint64_t size, uint32_t flags) const;
    [[nodiscard]] Status place(ClientTransport &wire, uint32_t buffer, uint64_t size, uint32_t flags,
                               const void *data);
    void                 forget(ClientTransport &wire, int32_t count, const uint32_t *buffers);
    void                 trim();

    bool     holds(uint32_t buffer) const;
    uint32_t map(uint32_t buffer, int64_t offset, int64_t length, uint32_t access, void *&pointer);
    uint32_t unmap(uint32_t buffer);
    uint32_t mark(uint32_t buffer, int64_t offset, int64_t length);
    uint32_t read(uint32_t buffer, int64_t offset, int64_t size, void *data);
    void    *pointer_of(uint32_t buffer) const;
    bool     mapping(uint32_t buffer, uint32_t &access, uint64_t &offset, uint64_t &bytes) const;
    bool     storage_flags(uint32_t buffer, int64_t &value) const;
    uint32_t held() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    struct Row {
        icd::StreamMemory::Slice slice_;
        uint64_t                 size_;
        uint32_t                 flags_;
        uint32_t                 access_;
        uint64_t                 map_offset_;
        uint64_t                 map_bytes_;
        HostMemoryRequest::Kind  kind_;
    };

    static constexpr uint32_t stream_kinds = 2u;

    icd::StreamMemory *stream_of(HostMemoryRequest::Kind kind);
    icd::StreamMemory *made_stream(HostMemoryRequest::Kind kind);
    uint32_t           stream_index(HostMemoryRequest::Kind kind) const;
    uint32_t           access_error(const Row &row, int64_t offset, int64_t length, uint32_t access) const;

    BlockSource                     &memory_;
    icd::HostChannel                &channel_;
    icd::GpuTimeline                &timeline_;
    ThreadLock                      &lock_;
    DriverLog                       &lines_;
    const void                      *owner_;
    GlPollClock                      clock_;
    ObjectStorage<icd::StreamMemory> streams_[stream_kinds];
    KeyedSlots<uint32_t, Row>        rows_;
    bool                             granted_;
    bool                             stopped_;

    friend struct SlicedBuffersProbe;
};

}

#endif
