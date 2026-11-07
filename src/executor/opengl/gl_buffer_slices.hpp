#ifndef AV_GL_BUFFER_SLICES_HPP
#define AV_GL_BUFFER_SLICES_HPP

#include "proto/generated/glmem/glmem_dispatch_server.h"
#include "proto/generated/opengl/gl_real_dispatch.h"

#include <stdint.h>

namespace av {

class DriverLog;
class GlBatchProgress;
class GlErrorQueue;
class GlHostImport;
class GlMemoryObjects;

class GlBufferSlices final : public glmemproto::ServerDispatch {
public:
    GlBufferSlices(GlMemoryObjects &memories, glproto::RealGl &gl, GlHostImport &import, const GlBatchProgress &progress,
                   GlErrorQueue &errors, const GuestBytes &guest, DriverLog &lines);
    GlBufferSlices(const GlBufferSlices &) = delete;
    GlBufferSlices &operator=(const GlBufferSlices &) = delete;

    void install();

    bool knows(uint32_t command_id) const;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply, WireArena &arena);

    bool on_QueryBufferSlicesGL(uint32_t &supported) override;
    bool on_PlaceBufferSlice(uint32_t buffer, uint64_t memory, uint64_t offset, uint64_t bytes,
                             uint32_t &placed) override;
    bool on_ForgetBufferSlice(uint32_t buffer) override;

    bool on_buffer_storage_mem(GLenum target, GLsizeiptr size, GLuint memory, GLuint64 offset);
    bool on_named_buffer_storage_mem(GLuint buffer, GLsizeiptr size, GLuint memory, GLuint64 offset);
    bool on_delete_memory_objects(GLsizei count, const GLuint *memories);
    bool on_delete_buffers(GLsizei count, const GLuint *buffers);

private:
    struct TargetBinding {
        uint32_t target_;
        uint32_t binding_;
    };

    static constexpr uint32_t binding_rows = 14u;
    static constexpr TargetBinding bindings_[binding_rows] = {
        {0x8892u, 0x8894u}, {0x8893u, 0x8895u}, {0x88EBu, 0x88EDu}, {0x88ECu, 0x88EFu}, {0x8A11u, 0x8A28u},
        {0x8C2Au, 0x8C2Bu}, {0x8C8Eu, 0x8C8Fu}, {0x8F36u, 0x8F36u}, {0x8F37u, 0x8F37u}, {0x8F3Fu, 0x8F43u},
        {0x90D2u, 0x90D3u}, {0x90EEu, 0x90EFu}, {0x9192u, 0x9193u}, {0x92C0u, 0x92C1u},
    };

    uint32_t bound_to(GLenum target);
    bool     object_ready(uint32_t buffer);
    bool     has_storage(uint32_t buffer);
    uint32_t store(uint32_t buffer, uint64_t bytes, uint32_t memory, uint64_t offset);

    glproto::RealGl &gl_;
    GlHostImport    &import_;
    const GlBatchProgress &progress_;
    GlErrorQueue    &errors_;
    DriverLog       &lines_;
    GlMemoryObjects &memories_;
};

}

#endif
