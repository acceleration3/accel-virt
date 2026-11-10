#ifndef AV_GLICD_MAPPED_BUFFERS_HPP
#define AV_GLICD_MAPPED_BUFFERS_HPP

#include "icd/common/once_latch.hpp"
#include "icd/common/open_mappings.hpp"
#include "icd/opengl/buffer_map_transfer.hpp"
#include "icd/opengl/buffer_mapping.hpp"
#include "icd/opengl/sliced_buffers.hpp"

#include "core/support/keyed_slots.hpp"

#include <stdint.h>

namespace av {
class DriverLog;
}

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}

namespace av::glicd {

class BufferShadows;
class SlicedBuffers;
struct MappedBuffersProbe;

class MappedBuffers {
public:
    static constexpr int64_t  whole_buffer = SlicedBuffers::whole_buffer;
    enum : uint32_t {
        gl_no_error = 0u, gl_invalid_value = 0x0501u, gl_invalid_operation = 0x0502u, gl_out_of_memory = 0x0505u,
        gl_buffer_size = 0x8764u, gl_read_only = 0x88B8u, gl_write_only = 0x88B9u, gl_read_write = 0x88BAu,
        gl_buffer_access = 0x88BBu, gl_buffer_mapped = 0x88BCu, gl_buffer_access_flags = 0x911Fu,
        gl_buffer_map_length = 0x9120u, gl_buffer_map_offset = 0x9121u, gl_buffer_storage_flags = 0x8220u,
    };

    MappedBuffers(BlockSource &memory, BufferShadows &shadows, SlicedBuffers &slices, DriverLog &log);
    ~MappedBuffers();
    MappedBuffers(const MappedBuffers &) = delete;
    MappedBuffers &operator=(const MappedBuffers &) = delete;

    uint32_t map(ClientTransport &wire, uint32_t buffer, int64_t offset, int64_t length, uint32_t access,
                 void *&pointer);
    uint32_t unmap(ClientTransport &wire, uint32_t buffer);
    uint32_t mark(uint32_t buffer, int64_t offset, int64_t length);
    void    *pointer_of(uint32_t buffer) const;
    bool     map_state(uint32_t buffer, uint32_t pname, int64_t &value) const;
    void     keep_flags(uint32_t buffer, uint32_t flags);
    bool     refuses_subdata(uint32_t buffer) const;
    void     flush_persistent(ClientTransport &wire);
    void     forget(int32_t count, const uint32_t *buffers);
    void     release();
    uint32_t held() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    BufferMapping       *find(uint32_t buffer);
    const BufferMapping *find(uint32_t buffer) const;
    void                 drop(BufferMapping &mapping);
    void                 log_once();

    BufferShadows                     &shadows_;
    SlicedBuffers                     &slices_;
    KeyedSlots<uint32_t, uint32_t>     kept_flags_;
    DriverLog                         &log_;
    BufferMapTransfer                  transfer_;
    icd::OpenMappings<BufferMapping>   mappings_;
    uint32_t                           persistent_writers_;
    icd::OnceLatch                 logged_;

    friend struct MappedBuffersProbe;
};

}

#endif
