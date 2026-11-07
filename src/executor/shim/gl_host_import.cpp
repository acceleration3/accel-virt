#include "executor/shim/gl_host_import.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/shim/gl_dma_buf_images.hpp"

namespace av {

namespace {
constexpr GLenum handle_type_opaque_fd = 0x9586u;
constexpr GLenum dedicated_memory_object = 0x9581u;
constexpr GLenum major_version = 0x821Bu;
constexpr GLenum minor_version = 0x821Cu;
constexpr GLint  first_version_with_named_storage = 45;

}

GlHostImport::GlHostImport()
    : create_memory_objects_(nullptr), import_memory_fd_(nullptr), memory_parameter_(nullptr),
      delete_memory_objects_(nullptr),
      gen_semaphores_(nullptr), import_semaphore_fd_(nullptr), delete_semaphores_(nullptr),
      get_integerv_(nullptr), extensions_(), errors_(), context_(nullptr), images_(nullptr),
      memory_(false), buffers_(false), fences_(false)
{
    AV_SCOPE("GlHostImport::GlHostImport");
}

void GlHostImport::attach(GlDmaBufImages &images)
{
    AV_SCOPE("GlHostImport::attach");
    images_ = &images;
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
    extensions_.configure(context);
    errors_.resolve(context);
    memory_ = create_memory_objects_ != nullptr && import_memory_fd_ != nullptr &&
              memory_parameter_ != nullptr && delete_memory_objects_ != nullptr && errors_.resolved() && lists("GL_EXT_memory_object_fd");
    buffers_ = memory_ && lists("GL_EXT_memory_object") && names_buffers();
    fences_ = gen_semaphores_ != nullptr && import_semaphore_fd_ != nullptr && delete_semaphores_ != nullptr &&
              errors_.resolved() && lists("GL_EXT_semaphore_fd");
}

void GlHostImport::give_back()
{
    AV_SCOPE("GlHostImport::give_back");
    context_ = nullptr;
    memory_ = false;
    buffers_ = false;
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
    return extensions_.lists(extension);
}

bool GlHostImport::imports_memory() const
{
    AV_SCOPE("GlHostImport::imports_memory");
    return memory_;
}

bool GlHostImport::names_buffers() const
{
    AV_SCOPE("GlHostImport::names_buffers");
    if (lists("GL_ARB_direct_state_access") || lists("GL_EXT_direct_state_access")) return true;
    if (get_integerv_ == nullptr) return false;
    GLint major = 0, minor = 0;
    get_integerv_(major_version, &major);
    get_integerv_(minor_version, &minor);
    return major * 10 + minor >= first_version_with_named_storage;
}

bool GlHostImport::places_buffers() const
{
    AV_SCOPE("GlHostImport::places_buffers");
    return buffers_;
}

bool GlHostImport::import_memory(int fd, uint64_t size, uint32_t &name)
{
    AV_SCOPE("GlHostImport::import_memory");
    return import_with(fd, size, 0, name);
}

bool GlHostImport::import_dedicated_memory(int fd, uint64_t size, uint32_t &name)
{
    AV_SCOPE("GlHostImport::import_dedicated_memory");
    return import_with(fd, size, 1, name);
}

bool GlHostImport::import_with(int fd, uint64_t size, GLint dedicated, uint32_t &name)
{
    AV_SCOPE("GlHostImport::import_with");
    OwnedFd taken;
    if (!memory_ || current() == nullptr || !taken.hold_copy(fd)) return false;
    GLuint made = 0u;
    create_memory_objects_(1, &made);
    static_cast<void>(errors_.clear());
    memory_parameter_(made, dedicated_memory_object, &dedicated);
    import_memory_fd_(made, static_cast<GLuint64>(size), handle_type_opaque_fd, taken.fd());
    if (!errors_.clear()) {
        delete_memory_objects_(1, &made);
        static_cast<void>(errors_.clear());
        return false;
    }
    static_cast<void>(taken.take());
    name = made;
    return true;
}

bool GlHostImport::import_semaphore(int fd, uint32_t &name)
{
    AV_SCOPE("GlHostImport::import_semaphore");
    OwnedFd taken;
    if (!fences_ || current() == nullptr || !taken.hold_copy(fd)) return false;
    GLuint made = 0u;
    gen_semaphores_(1, &made);
    static_cast<void>(errors_.clear());
    import_semaphore_fd_(made, handle_type_opaque_fd, taken.fd());
    if (!errors_.clear()) {
        delete_semaphores_(1, &made);
        static_cast<void>(errors_.clear());
        return false;
    }
    static_cast<void>(taken.take());
    name = made;
    return true;
}

void GlHostImport::delete_memory(uint32_t name)
{
    AV_SCOPE("GlHostImport::delete_memory");
    if (images_ != nullptr) images_->forget(name);
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

}
