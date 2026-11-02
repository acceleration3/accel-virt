#include "executor/shim/gl_shared_buffer.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/shim/gl_host_import.hpp"

namespace av {

namespace {

constexpr GLenum buffer_size = 0x8764u;

}

GlSharedBuffer::GlSharedBuffer()
    : create_buffers_(nullptr), storage_mem_(nullptr), buffer_parameter_(nullptr), delete_buffers_(nullptr),
      errors_(), context_(nullptr), import_(nullptr)
{
    AV_SCOPE("GlSharedBuffer::GlSharedBuffer");
}

void GlSharedBuffer::configure(GlContext &context, GlHostImport &import)
{
    AV_SCOPE("GlSharedBuffer::configure");
    context_ = &context;
    import_ = &import;
    if (!context.make_current()) return;
    create_buffers_ = reinterpret_cast<void(APIENTRY *)(GLsizei, GLuint *)>(context.resolve("glCreateBuffers"));
    storage_mem_ = reinterpret_cast<void(APIENTRY *)(GLuint, GLsizeiptr, GLuint, GLuint64)>(
        context.resolve("glNamedBufferStorageMemEXT"));
    buffer_parameter_ = reinterpret_cast<void(APIENTRY *)(GLuint, GLenum, GLint64 *)>(
        context.resolve("glGetNamedBufferParameteri64v"));
    delete_buffers_ =
        reinterpret_cast<void(APIENTRY *)(GLsizei, const GLuint *)>(context.resolve("glDeleteBuffers"));
    errors_.resolve(context);
}

void GlSharedBuffer::give_back()
{
    AV_SCOPE("GlSharedBuffer::give_back");
    context_ = nullptr;
    import_ = nullptr;
}

bool GlSharedBuffer::makes_buffers() const
{
    AV_SCOPE("GlSharedBuffer::makes_buffers");
    return import_ != nullptr && import_->imports_memory() && create_buffers_ != nullptr &&
           storage_mem_ != nullptr && buffer_parameter_ != nullptr && delete_buffers_ != nullptr &&
           errors_.resolved();
}

GlContext *GlSharedBuffer::current()
{
    AV_SCOPE("GlSharedBuffer::current");
    if (context_ == nullptr || !context_->make_current()) return nullptr;
    return context_;
}

bool GlSharedBuffer::make(int fd, uint64_t size, uint32_t &memory, uint32_t &buffer)
{
    AV_SCOPE("GlSharedBuffer::make");
    if (!makes_buffers() || size == 0u || size > static_cast<uint64_t>(INT64_MAX) || current() == nullptr)
        return false;
    uint32_t imported = 0u;
    if (!import_->import_memory(fd, size, imported)) return false;
    GLuint made = 0u;
    create_buffers_(1, &made);
    GLint64 stored = 0;
    if (made != 0u) {
        storage_mem_(made, static_cast<GLsizeiptr>(size), imported, 0u);
        buffer_parameter_(made, buffer_size, &stored);
    }
    if (stored != static_cast<GLint64>(size)) {
        if (made != 0u) {
            static_cast<void>(errors_.clear());
            delete_buffers_(1, &made);
        }
        import_->delete_memory(imported);
        return false;
    }
    memory = imported;
    buffer = made;
    return true;
}

void GlSharedBuffer::destroy(uint32_t memory, uint32_t buffer)
{
    AV_SCOPE("GlSharedBuffer::destroy");
    if (current() == nullptr) return;
    if (buffer != 0u && delete_buffers_ != nullptr) {
        const GLuint named = buffer;
        delete_buffers_(1, &named);
    }
    if (memory != 0u && import_ != nullptr) import_->delete_memory(memory);
}

}
