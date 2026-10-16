#include "executor/shim/gl_host_import.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_context.hpp"

#include <string.h>
#include <unistd.h>

namespace av {

namespace {

/* The only handle type GL_EXT_memory_object_fd and GL_EXT_semaphore_fd define.
 * On the Mesa stack this device runs against, an "opaque" fd of that type is a
 * dma-buf, which is why the device refuses to hand GL a descriptor Vulkan
 * exported as an opaque fd: that one is a driver-private handle of another
 * API and GL cannot read it (research.md §7). */
constexpr GLenum handle_type_opaque_fd = 0x9586u;
/* GL_DEDICATED_MEMORY_OBJECT_EXT. */
constexpr GLenum dedicated_memory_object = 0x9581u;
constexpr GLenum num_extensions = 0x821Du;
constexpr GLenum extensions_name = 0x1F03u;

}  // namespace

GlHostImport::GlHostImport()
    : create_memory_objects_(nullptr), import_memory_fd_(nullptr), memory_parameter_(nullptr),
      delete_memory_objects_(nullptr),
      gen_semaphores_(nullptr), import_semaphore_fd_(nullptr), delete_semaphores_(nullptr),
      get_integerv_(nullptr), get_stringi_(nullptr), errors_(), context_(nullptr),
      memory_(false), fences_(false)
{
    AV_SCOPE("GlHostImport::GlHostImport");
}

void GlHostImport::configure(GlContext &context)
{
    AV_SCOPE("GlHostImport::configure");
    context_ = &context;
    if (!context.make_current()) return;
    create_memory_objects_ =
        reinterpret_cast<void(APIENTRY *)(GLsizei, GLuint *)>(context.resolve("glCreateMemoryObjectsEXT"));
    import_memory_fd_ =
        reinterpret_cast<void(APIENTRY *)(GLuint, GLuint64, GLenum, GLint)>(context.resolve("glImportMemoryFdEXT"));
    memory_parameter_ = reinterpret_cast<void(APIENTRY *)(GLuint, GLenum, const GLint *)>(
        context.resolve("glMemoryObjectParameterivEXT"));
    delete_memory_objects_ =
        reinterpret_cast<void(APIENTRY *)(GLsizei, const GLuint *)>(context.resolve("glDeleteMemoryObjectsEXT"));
    gen_semaphores_ = reinterpret_cast<void(APIENTRY *)(GLsizei, GLuint *)>(context.resolve("glGenSemaphoresEXT"));
    import_semaphore_fd_ =
        reinterpret_cast<void(APIENTRY *)(GLuint, GLenum, GLint)>(context.resolve("glImportSemaphoreFdEXT"));
    delete_semaphores_ =
        reinterpret_cast<void(APIENTRY *)(GLsizei, const GLuint *)>(context.resolve("glDeleteSemaphoresEXT"));
    get_integerv_ = reinterpret_cast<void(APIENTRY *)(GLenum, GLint *)>(context.resolve("glGetIntegerv"));
    get_stringi_ = reinterpret_cast<const GLubyte *(APIENTRY *)(GLenum, GLuint)>(context.resolve("glGetStringi"));
    errors_.resolve(context);
    memory_ = create_memory_objects_ != nullptr && import_memory_fd_ != nullptr && memory_parameter_ != nullptr &&
              delete_memory_objects_ != nullptr && errors_.resolved() && lists("GL_EXT_memory_object_fd");
    fences_ = gen_semaphores_ != nullptr && import_semaphore_fd_ != nullptr && delete_semaphores_ != nullptr &&
              errors_.resolved() && lists("GL_EXT_semaphore_fd");
}

void GlHostImport::give_back()
{
    AV_SCOPE("GlHostImport::give_back");
    context_ = nullptr;
    memory_ = false;
    fences_ = false;
}

GlContext *GlHostImport::current()
{
    AV_SCOPE("GlHostImport::current");
    if (context_ == nullptr || !context_->make_current()) return nullptr;
    return context_;
}

bool GlHostImport::lists(const char *extension) const
{
    AV_SCOPE("GlHostImport::lists");
    if (get_integerv_ == nullptr || get_stringi_ == nullptr) return false;
    GLint count = 0;
    get_integerv_(num_extensions, &count);
    for (GLint i = 0; i < count; i++) {
        const GLubyte *const name = get_stringi_(extensions_name, static_cast<GLuint>(i));
        if (name != nullptr && strcmp(reinterpret_cast<const char *>(name), extension) == 0) return true;
    }
    return false;
}

bool GlHostImport::imports_memory() const
{
    AV_SCOPE("GlHostImport::imports_memory");
    return memory_;
}

bool GlHostImport::import_memory(int fd, uint64_t size, uint32_t &name)
{
    AV_SCOPE("GlHostImport::import_memory");
    if (!memory_ || current() == nullptr) return false;
    const int taken = dup(fd);
    if (taken < 0) return false;
    GLuint made = 0u;
    create_memory_objects_(1, &made);
    static_cast<void>(errors_.clear());
    /* Shared memory is one image's own allocation - a Direct3D surface's, or a
     * Vulkan dedicated one - and radeonsi reads such an image's layout from the
     * dma-buf only when told it is dedicated: otherwise it lays the texture out
     * its own way over memory RADV laid out, and every texel lands elsewhere
     * (test_d3d_gl_interop_device). */
    const GLint dedicated = 1;
    memory_parameter_(made, dedicated_memory_object, &dedicated);
    import_memory_fd_(made, static_cast<GLuint64>(size), handle_type_opaque_fd, taken);
    if (!errors_.clear()) {
        delete_memory_objects_(1, &made);
        static_cast<void>(errors_.clear());
        close(taken);
        return false;
    }
    /* A successful glImportMemoryFdEXT owns the descriptor and GL closes it. */
    name = made;
    return true;
}

bool GlHostImport::import_semaphore(int fd, uint32_t &name)
{
    AV_SCOPE("GlHostImport::import_semaphore");
    if (!fences_ || current() == nullptr) return false;
    const int taken = dup(fd);
    if (taken < 0) return false;
    GLuint made = 0u;
    gen_semaphores_(1, &made);
    static_cast<void>(errors_.clear());
    import_semaphore_fd_(made, handle_type_opaque_fd, taken);
    if (!errors_.clear()) {
        delete_semaphores_(1, &made);
        static_cast<void>(errors_.clear());
        close(taken);
        return false;
    }
    name = made;
    return true;
}

void GlHostImport::delete_memory(uint32_t name)
{
    AV_SCOPE("GlHostImport::delete_memory");
    if (!memory_ || current() == nullptr) return;
    const GLuint named = name;
    delete_memory_objects_(1, &named);
    static_cast<void>(errors_.clear());
}

void GlHostImport::delete_semaphore(uint32_t name)
{
    AV_SCOPE("GlHostImport::delete_semaphore");
    if (!fences_ || current() == nullptr) return;
    const GLuint named = name;
    delete_semaphores_(1, &named);
    static_cast<void>(errors_.clear());
}

}  // namespace av
