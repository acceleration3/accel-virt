/* A shared descriptor turned into an OpenGL memory object or semaphore, and
 * back (feature 053).
 *
 * The driver's half of OpenGlBackend::import_object. Everything that decides
 * whether an import should happen - the object's kind, the descriptor's kind,
 * the size, and which names this context made - stays in the backend where a
 * test can judge it; what is here is the six entry points GL_EXT_memory_object_fd
 * and GL_EXT_semaphore_fd add, which no unit test can call without a driver.
 *
 * The entry points are resolved rather than taken from the generated table:
 * the guest never sends glCreateMemoryObjectsEXT or glImportMemoryFdEXT - the
 * device makes the object inside the import and answers its name - so they are
 * deliberately absent from the protocol (contracts/wire_additions.md). */
#ifndef AV_GL_HOST_IMPORT_HPP
#define AV_GL_HOST_IMPORT_HPP

#include "proto/generated/opengl/gl_types.h"

#include <stdint.h>

namespace av {

class GlContext;

class GlHostImport {
public:
    GlHostImport();
    GlHostImport(const GlHostImport &) = delete;
    GlHostImport &operator=(const GlHostImport &) = delete;

    /* Keeps `context`, which must outlive this, makes it current, resolves
     * what the two fd extensions need, and reads the host's extension list to
     * see whether it has them at all. A host with neither leaves this
     * importing nothing, which is an answer and not a failure: the device
     * refuses the guest's open with a status.
     *
     * Every call below makes the context current itself, so nothing above has
     * to ask whether a context that is closing still has one - which is a
     * question only a driver can answer. */
    void configure(GlContext &context);
    /* Lets go of the context, which is what a shut down does before closing
     * it. Imports nothing afterwards. */
    void give_back();
    /* Whether this host lists GL_EXT_memory_object_fd and resolved everything
     * a memory import needs, and the same for GL_EXT_semaphore_fd. */
    bool imports_memory() const;
    bool imports_fences() const;

    /* `size` bytes of `fd` as a memory object of the current context, named in
     * `name`. GL takes the descriptor on success, so what is handed to it is a
     * duplicate and the caller's stays the caller's - a shared object's
     * descriptor outlives every import of it. Nothing is left behind unless it
     * answers true. */
    [[nodiscard]] bool import_memory(int fd, uint64_t size, uint32_t &name);
    /* The same for a semaphore, whose payload `fd` carries. */
    [[nodiscard]] bool import_semaphore(int fd, uint32_t &name);
    void               delete_memory(uint32_t name);
    void               delete_semaphore(uint32_t name);

private:
    /* Whether the current context's extension list has `extension`. */
    bool lists(const char *extension) const;
    /* The context, made current, or null when there is none to make. */
    GlContext *current();

    void (APIENTRY *create_memory_objects_)(GLsizei, GLuint *);
    void (APIENTRY *import_memory_fd_)(GLuint, GLuint64, GLenum, GLint);
    void (APIENTRY *memory_parameter_)(GLuint, GLenum, const GLint *);
    void (APIENTRY *delete_memory_objects_)(GLsizei, const GLuint *);
    void (APIENTRY *gen_semaphores_)(GLsizei, GLuint *);
    void (APIENTRY *import_semaphore_fd_)(GLuint, GLenum, GLint);
    void (APIENTRY *delete_semaphores_)(GLsizei, const GLuint *);
    void (APIENTRY *get_integerv_)(GLenum, GLint *);
    const GLubyte *(APIENTRY *get_stringi_)(GLenum, GLuint);
    GLenum (APIENTRY *get_error_)();
    GlContext *context_;
    bool memory_;
    bool fences_;
};

}  // namespace av

#endif  // AV_GL_HOST_IMPORT_HPP
