/* The buffer ranges one context has mapped (data-model.md: at most 64), with
 * the checks OpenGL makes of a map, an unmap and an explicit flush, and the
 * one line per context that says maps are copies here (FR-006). The host is
 * asked a buffer's size at every map, since a range is judged against it. */
#ifndef AV_GLICD_MAPPED_BUFFERS_HPP
#define AV_GLICD_MAPPED_BUFFERS_HPP

#include "core/support/object_storage.hpp"
#include "icd/opengl/buffer_mapping.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::icd {
class LogSink;
}

namespace av::glicd {

struct MappedBuffersProbe;

class MappedBuffers {
public:
    static constexpr uint32_t max_mappings = 64u;
    /* glMapBuffer's length: all of the buffer. */
    static constexpr int64_t  whole_buffer = -1;
    enum : uint32_t {
        gl_no_error = 0u, gl_invalid_value = 0x0501u, gl_invalid_operation = 0x0502u, gl_out_of_memory = 0x0505u,
        gl_buffer_size = 0x8764u,
    };

    MappedBuffers(BlockSource &memory, icd::LogSink &log);
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
    /* The slot mapping `buffer`; for 0, the first free slot. */
    uint32_t find(uint32_t buffer) const;
    void     drop(uint32_t slot);
    void     log_once();

    BlockSource                   &memory_;
    icd::LogSink                  &log_;
    ObjectStorage<BufferMapping>   mappings_[max_mappings];
    uint32_t                       persistent_writers_;
    bool                           logged_;

    friend struct MappedBuffersProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_MAPPED_BUFFERS_HPP
