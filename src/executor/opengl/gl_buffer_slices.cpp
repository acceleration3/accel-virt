#include "executor/opengl/gl_buffer_slices.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/opengl/gl_batch_progress.hpp"
#include "executor/opengl/gl_error_queue.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "proto/generated/glmem/glmem_command_names.h"

namespace av {

namespace {

constexpr uint32_t gl_invalid_value = 0x0501u;
constexpr uint32_t gl_out_of_memory = 0x0505u;
constexpr GLenum   gl_copy_write_buffer = 0x8F37u;
constexpr GLenum   gl_buffer_immutable_storage = 0x821Fu;

bool hook_buffer_storage_mem(void *user, GLenum target, GLsizeiptr size, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_buffer_storage_mem");
    return static_cast<GlBufferSlices *>(user)->on_buffer_storage_mem(target, size, memory, offset);
}

bool hook_named_buffer_storage_mem(void *user, GLuint buffer, GLsizeiptr size, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_named_buffer_storage_mem");
    return static_cast<GlBufferSlices *>(user)->on_named_buffer_storage_mem(buffer, size, memory, offset);
}

bool hook_delete_memory_objects(void *user, GLsizei count, const GLuint *memories)
{
    AV_SCOPE("hook_delete_memory_objects");
    return static_cast<GlBufferSlices *>(user)->on_delete_memory_objects(count, memories);
}

bool hook_delete_buffers(void *user, GLsizei count, const GLuint *buffers)
{
    AV_SCOPE("hook_delete_buffers");
    return static_cast<GlBufferSlices *>(user)->on_delete_buffers(count, buffers);
}

}

GlBufferSlices::GlBufferSlices(BlockSource &blocks, glproto::RealGl &gl, GlHostImport &import,
                               const GlBatchProgress &progress, GlErrorQueue &errors, const GuestBytes &guest,
                               DriverLog &lines)
    : glmemproto::ServerDispatch(guest), gl_(gl), import_(import), progress_(progress), errors_(errors),
      lines_(lines), memories_(blocks, progress)
{
    AV_SCOPE("GlBufferSlices::GlBufferSlices");
}

void GlBufferSlices::install()
{
    AV_SCOPE("GlBufferSlices::install");
    gl_.av_hook_user = this;
    gl_.av_hook_glBufferStorageMemEXT = hook_buffer_storage_mem;
    gl_.av_hook_glNamedBufferStorageMemEXT = hook_named_buffer_storage_mem;
    gl_.av_hook_glDeleteMemoryObjectsEXT = hook_delete_memory_objects;
    gl_.av_hook_glDeleteBuffers = hook_delete_buffers;
}

bool GlBufferSlices::knows(uint32_t command_id) const
{
    AV_SCOPE("GlBufferSlices::knows");
    return glmemproto::knows_command(command_id);
}

bool GlBufferSlices::run(uint32_t command_id, WireReader &payload, WireWriter &reply, WireArena &arena)
{
    AV_SCOPE("GlBufferSlices::run");
    if (glmemproto::dispatch_command(*this, command_id, payload, reply, arena)) return true;
    LogLine(lines_).text(glmemproto::command_name(command_id)).text(" could not run").write();
    return false;
}

Status GlBufferSlices::add_memory(uint32_t name, uint64_t size)
{
    AV_SCOPE("GlBufferSlices::add_memory");
    return memories_.add(name, size);
}

void GlBufferSlices::retire_memory(uint32_t name)
{
    AV_SCOPE("GlBufferSlices::retire_memory");
    memories_.retire(name);
}

void GlBufferSlices::reclaim()
{
    AV_SCOPE("GlBufferSlices::reclaim");
    memories_.reclaim(import_);
}

void GlBufferSlices::destroy_all()
{
    AV_SCOPE("GlBufferSlices::destroy_all");
    memories_.destroy_all(import_);
}

bool GlBufferSlices::on_QueryBufferSlicesGL(uint32_t &supported)
{
    AV_SCOPE("GlBufferSlices::on_QueryBufferSlicesGL");
    supported = import_.places_buffers() && gl_.glNamedBufferStorageMemEXT != nullptr && progress_.fences() ? 1u : 0u;
    return true;
}

bool GlBufferSlices::on_PlaceBufferSlice(uint32_t buffer, uint64_t memory, uint64_t offset, uint64_t bytes,
                                         uint32_t &placed)
{
    AV_SCOPE("GlBufferSlices::on_PlaceBufferSlice");
    placed = 0u;
    if (!import_.places_buffers() || gl_.glNamedBufferStorageMemEXT == nullptr || memory > UINT32_MAX) return true;
    const Status admitted = memories_.admit(buffer, static_cast<uint32_t>(memory), offset, bytes);
    if (admitted != Status::ok) {
        LogLine(lines_).text("a buffer slice was refused, status ").number(static_cast<uint32_t>(admitted)).write();
        return true;
    }
    if (!object_ready(buffer) || has_storage(buffer)) return true;
    const uint32_t error = store(buffer, bytes, static_cast<uint32_t>(memory), offset);
    if (error != 0u) {
        LogLine(lines_).text("the host GL refused a buffer slice, error ").number(error).write();
        return true;
    }
    placed = 1u;
    return true;
}

bool GlBufferSlices::on_ForgetBufferSlice(uint32_t buffer)
{
    AV_SCOPE("GlBufferSlices::on_ForgetBufferSlice");
    memories_.leave(buffer);
    return true;
}

bool GlBufferSlices::on_named_buffer_storage_mem(GLuint buffer, GLsizeiptr size, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlBufferSlices::on_named_buffer_storage_mem");
    const uint32_t error = size > 0 ? store(buffer, static_cast<uint64_t>(size), memory, offset) : gl_invalid_value;
    if (error != 0u) errors_.record(error);
    return false;
}

bool GlBufferSlices::on_buffer_storage_mem(GLenum target, GLsizeiptr size, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlBufferSlices::on_buffer_storage_mem");
    const uint32_t buffer = bound_to(target);
    if (buffer == 0u) return true;
    return on_named_buffer_storage_mem(buffer, size, memory, offset);
}

bool GlBufferSlices::on_delete_memory_objects(GLsizei count, const GLuint *memories)
{
    AV_SCOPE("GlBufferSlices::on_delete_memory_objects");
    for (GLsizei at = 0; memories != nullptr && at < count; at++) memories_.retire(memories[at]);
    memories_.reclaim(import_);
    return false;
}

bool GlBufferSlices::on_delete_buffers(GLsizei count, const GLuint *buffers)
{
    AV_SCOPE("GlBufferSlices::on_delete_buffers");
    for (GLsizei at = 0; buffers != nullptr && at < count; at++) memories_.leave(buffers[at]);
    return true;
}

uint32_t GlBufferSlices::bound_to(GLenum target)
{
    AV_SCOPE("GlBufferSlices::bound_to");
    if (gl_.glGetIntegerv == nullptr) return 0u;
    for (const TargetBinding &row : bindings_) {
        if (row.target_ != target) continue;
        GLint bound = 0;
        gl_.glGetIntegerv(row.binding_, &bound);
        return static_cast<uint32_t>(bound);
    }
    return 0u;
}

bool GlBufferSlices::object_ready(uint32_t buffer)
{
    AV_SCOPE("GlBufferSlices::object_ready");
    if (gl_.glIsBuffer == nullptr || gl_.glBindBuffer == nullptr || gl_.glGetIntegerv == nullptr) return false;
    if (gl_.glIsBuffer(buffer) != 0u) return true;
    errors_.collect();
    GLint previous = 0;
    gl_.glGetIntegerv(gl_copy_write_buffer, &previous);
    gl_.glBindBuffer(gl_copy_write_buffer, buffer);
    gl_.glBindBuffer(gl_copy_write_buffer, static_cast<GLuint>(previous));
    const uint32_t error = errors_.take();
    errors_.collect();
    return error == 0u && gl_.glIsBuffer(buffer) != 0u;
}

bool GlBufferSlices::has_storage(uint32_t buffer)
{
    AV_SCOPE("GlBufferSlices::has_storage");
    auto *const query = gl_.glGetNamedBufferParameteriv != nullptr ? gl_.glGetNamedBufferParameteriv
                                                                   : gl_.glGetNamedBufferParameterivEXT;
    if (query == nullptr) return true;
    errors_.collect();
    GLint immutable = 1;
    query(buffer, gl_buffer_immutable_storage, &immutable);
    const uint32_t error = errors_.take();
    errors_.collect();
    return error != 0u || immutable != 0;
}

uint32_t GlBufferSlices::store(uint32_t buffer, uint64_t bytes, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("GlBufferSlices::store");
    const Status admitted = memories_.admit(buffer, memory, offset, bytes);
    if (admitted != Status::ok) return admitted == Status::no_memory ? gl_out_of_memory : gl_invalid_value;
    errors_.collect();
    gl_.glNamedBufferStorageMemEXT(buffer, static_cast<GLsizeiptr>(bytes), memory, offset);
    const uint32_t error = errors_.take();
    errors_.collect();
    if (error != 0u) return error;
    const Status placed = memories_.place(buffer, memory, offset, bytes);
    return placed == Status::ok ? 0u : gl_out_of_memory;
}

uint32_t GlBufferSlices::count() const
{
    AV_SCOPE("GlBufferSlices::count");
    return memories_.count();
}

uint32_t GlBufferSlices::placed() const
{
    AV_SCOPE("GlBufferSlices::placed");
    return memories_.placed();
}

bool GlBufferSlices::invariants_hold() const
{
    AV_SCOPE("GlBufferSlices::invariants_hold");
    return memories_.invariants_hold();
}

}
