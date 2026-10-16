/* The buffer ranges one context has mapped, as many as it maps, with
 * the checks OpenGL makes of a map, an unmap and an explicit flush, and the
 * one line per context that says maps are copies here (FR-006). A range is
 * judged against the buffer's size, which the host is asked only when the
 * context does not know it; what each map starts from and sends back is
 * weighed against the context's copies of what the host holds
 * (BufferShadows). */
#ifndef AV_GLICD_MAPPED_BUFFERS_HPP
#define AV_GLICD_MAPPED_BUFFERS_HPP

#include "icd/common/once_latch.hpp"
#include "icd/common/open_mappings.hpp"
#include "icd/opengl/buffer_map_transfer.hpp"
#include "icd/opengl/buffer_mapping.hpp"

#include <stdint.h>

namespace av {
class DriverLog;
}  // namespace av

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::glicd {

class BufferShadows;
struct MappedBuffersProbe;

class MappedBuffers {
public:
    /* glMapBuffer's length: all of the buffer. */
    static constexpr int64_t  whole_buffer = -1;
    enum : uint32_t {
        gl_no_error = 0u, gl_invalid_value = 0x0501u, gl_invalid_operation = 0x0502u, gl_out_of_memory = 0x0505u,
        gl_buffer_size = 0x8764u, gl_read_only = 0x88B8u, gl_write_only = 0x88B9u, gl_read_write = 0x88BAu,
        gl_buffer_access = 0x88BBu, gl_buffer_mapped = 0x88BCu, gl_buffer_access_flags = 0x911Fu,
        gl_buffer_map_length = 0x9120u, gl_buffer_map_offset = 0x9121u,
    };

    MappedBuffers(BlockSource &memory, BufferShadows &shadows, DriverLog &log);
    ~MappedBuffers();
    MappedBuffers(const MappedBuffers &) = delete;
    MappedBuffers &operator=(const MappedBuffers &) = delete;

    /* glMapNamedBufferRange: the GL error, and the application's pointer in
     * `pointer` when there is none. */
    uint32_t map(ClientTransport &wire, uint32_t buffer, int64_t offset, int64_t length, uint32_t access,
                 void *&pointer);
    /* glUnmapNamedBuffer: what was written goes back, then the range is
     * given up. */
    uint32_t unmap(ClientTransport &wire, uint32_t buffer);
    /* glFlushMappedNamedBufferRange. */
    uint32_t mark(uint32_t buffer, int64_t offset, int64_t length);
    /* GL_BUFFER_MAP_POINTER: null for a buffer that is not mapped. */
    void    *pointer_of(uint32_t buffer) const;
    /* GL_BUFFER_MAPPED, _ACCESS, _ACCESS_FLAGS, _MAP_OFFSET and _MAP_LENGTH
     * of `buffer` (0: of no buffer): a map the host never saw, so only the
     * guest can answer. An unmapped buffer answers as the host would - it
     * never maps one. False, with `value` untouched, for any other name. */
    bool     map_state(uint32_t buffer, uint32_t pname, int64_t &value) const;
    /* Every persistent write map to the host. */
    void     flush_persistent(ClientTransport &wire);
    /* Deleted buffers are unmapped with nothing written, as OpenGL does. */
    void     forget(int32_t count, const uint32_t *buffers);
    /* Every range given up with nothing written: the context is going. */
    void     release();
    uint32_t held() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    /* The mapping of `buffer`, or null; 0 maps nothing. */
    BufferMapping       *find(uint32_t buffer);
    const BufferMapping *find(uint32_t buffer) const;
    void                 drop(BufferMapping &mapping);
    void                 log_once();

    BufferShadows                     &shadows_;
    DriverLog                         &log_;
    BufferMapTransfer                  transfer_;
    /* Each mapping by the buffer it maps: OpenGL maps a buffer once. */
    icd::OpenMappings<BufferMapping>   mappings_;
    uint32_t                           persistent_writers_;
    icd::OnceLatch                 logged_;

    friend struct MappedBuffersProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_MAPPED_BUFFERS_HPP
