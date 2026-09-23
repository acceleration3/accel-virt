#include "device/shim/gl_shared_buffer.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/gl_context.hpp"

namespace av {

namespace {

/* GL_ARRAY_BUFFER is a binding point and nothing more here: the buffer is
 * storage OpenCL names, and nothing ever draws from it. */
constexpr GLenum array_buffer = 0x8892u;
constexpr GLenum no_error = 0u;

}  // namespace

GlSharedBuffer::GlSharedBuffer()
    : gen_buffers_(nullptr), bind_buffer_(nullptr), buffer_storage_mem_(nullptr), delete_buffers_(nullptr),
      get_error_(nullptr), context_(nullptr)
{
    AV_SCOPE("GlSharedBuffer::GlSharedBuffer");
}

void GlSharedBuffer::configure(GlContext &context)
{
    AV_SCOPE("GlSharedBuffer::configure");
    context_ = &context;
    if (!context.make_current()) return;
    gen_buffers_ = reinterpret_cast<void(APIENTRY *)(GLsizei, GLuint *)>(context.resolve("glGenBuffers"));
    bind_buffer_ = reinterpret_cast<void(APIENTRY *)(GLenum, GLuint)>(context.resolve("glBindBuffer"));
    buffer_storage_mem_ = reinterpret_cast<void(APIENTRY *)(GLenum, GLsizeiptr, GLuint, GLuint64)>(
        context.resolve("glBufferStorageMemEXT"));
    delete_buffers_ =
        reinterpret_cast<void(APIENTRY *)(GLsizei, const GLuint *)>(context.resolve("glDeleteBuffers"));
    get_error_ = reinterpret_cast<GLenum(APIENTRY *)()>(context.resolve("glGetError"));
}

void GlSharedBuffer::give_back()
{
    AV_SCOPE("GlSharedBuffer::give_back");
    context_ = nullptr;
}

bool GlSharedBuffer::makes_buffers() const
{
    AV_SCOPE("GlSharedBuffer::makes_buffers");
    return gen_buffers_ != nullptr && bind_buffer_ != nullptr && buffer_storage_mem_ != nullptr &&
           delete_buffers_ != nullptr && get_error_ != nullptr;
}

GlContext *GlSharedBuffer::current()
{
    AV_SCOPE("GlSharedBuffer::current");
    if (context_ == nullptr || !context_->make_current()) return nullptr;
    return context_;
}

bool GlSharedBuffer::make(uint32_t memory, uint64_t size, uint32_t &buffer)
{
    AV_SCOPE("GlSharedBuffer::make");
    if (!makes_buffers() || memory == 0u || size == 0u || current() == nullptr) return false;
    GLuint made = 0u;
    gen_buffers_(1, &made);
    while (get_error_() != no_error) {}
    bind_buffer_(array_buffer, made);
    buffer_storage_mem_(array_buffer, static_cast<GLsizeiptr>(size), memory, 0u);
    if (get_error_() != no_error) {
        bind_buffer_(array_buffer, 0u);
        delete_buffers_(1, &made);
        while (get_error_() != no_error) {}
        return false;
    }
    /* Unbound at once: the binding was only how the storage is named, and a
     * buffer left bound would be state the next import has to work around. */
    bind_buffer_(array_buffer, 0u);
    buffer = made;
    return true;
}

void GlSharedBuffer::destroy(uint32_t buffer)
{
    AV_SCOPE("GlSharedBuffer::destroy");
    if (buffer == 0u || !makes_buffers() || current() == nullptr) return;
    const GLuint named = buffer;
    delete_buffers_(1, &named);
    while (get_error_() != no_error) {}
}

}  // namespace av
