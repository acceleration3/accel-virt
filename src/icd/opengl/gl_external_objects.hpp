/* The memory objects and semaphores one GL context imported from a Win32
 * handle (feature 053, US2).
 *
 * An OpenGL name on this wire is the *host's* own: glGenBuffers asks the host
 * and hands the application what the host chose, and nothing translates a name
 * on the way past. A shared object cannot work that way. The application asks
 * for a name with glCreateMemoryObjectsEXT before it has anything to import,
 * and the host's name does not exist until the device has opened the object -
 * so this hands out a name of its own at the create, learns the host's at the
 * import, and the entry points that use the name afterwards substitute one for
 * the other. That is the whole reason this class exists rather than the six
 * generated commands going straight through.
 *
 * The two GL namespaces are separate - memory object 3 and semaphore 3 are
 * different objects - so every row carries which it is, and a name looked up
 * in the wrong one is not found.
 *
 * One per guest context, because a name belongs to the context that made it. */
#ifndef AV_GLICD_GL_EXTERNAL_OBJECTS_HPP
#define AV_GLICD_GL_EXTERNAL_OBJECTS_HPP

#include <stdint.h>

namespace av {
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::icd {
class LogSink;
class SharedHandles;
}  // namespace av::icd

namespace av::glicd {

struct GlExternalObjectsProbe;

/* One name this ICD handed out, and what the device made of it. A type of its
 * own so the table's rows are complete (tools/header_scan.py reads a class's
 * members only to its first nested type). */
struct GlSharedObjectRow {
    /* What the application holds, which this ICD chose. Never 0. */
    uint32_t name_;
    /* The host's own name, learnt at the import; 0 for a name created and not
     * yet imported, which every use of must refuse. */
    uint32_t made_;
    /* What the object is called on the wire; 0 before the import. */
    uint64_t object_id_;
    /* The bytes it was opened as, which a release names again: whole pages for
     * memory, and 0 for a semaphore, as the wire body demands of each. */
    uint64_t size_;
    /* Which GL namespace the name is in: memory, or a semaphore. */
    bool     memory_;
    /* GL_DEDICATED_MEMORY_OBJECT_EXT and GL_PROTECTED_MEMORY_OBJECT_EXT as the
     * application set them, which is always before the import - so they are
     * kept here and read by the import, which refuses what it cannot give. */
    bool     dedicated_;
    bool     protected_;
};

class GlExternalObjects {
public:
    /* As many as a context can share, matching icd::SharedHandles: a name
     * whose handle the process could not hold is a name it could not use. */
    static constexpr uint32_t max_objects = 64u;
    /* The GL errors it answers, which are the ones these entry points have. */
    enum : uint32_t {
        gl_no_error = 0u,
        gl_invalid_enum = 0x0500u,
        gl_invalid_value = 0x0501u,
        gl_invalid_operation = 0x0502u,
        gl_out_of_memory = 0x0505u,
    };
    /* GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, the one type these imports take: the
     * KMT and D3D spellings name objects nothing here turns into a host one. */
    static constexpr uint32_t opaque_win32 = 0x9587u;
    /* The parameters the two extensions define. A memory object has two and a
     * semaphore has one - GL_D3D12_FENCE_VALUE_EXT, which names the value of a
     * D3D12 fence and is refused, because nothing here is imported from one. */
    static constexpr uint32_t dedicated = 0x9581u;
    static constexpr uint32_t protected_memory = 0x959Bu;
    static constexpr uint32_t d3d12_fence_value = 0x9595u;

    GlExternalObjects(icd::SharedHandles &handles, icd::LogSink &log);
    GlExternalObjects(const GlExternalObjects &) = delete;
    GlExternalObjects &operator=(const GlExternalObjects &) = delete;

    /* glCreateMemoryObjectsEXT and glGenSemaphoresEXT, answered here and never
     * sent: the host has nothing to name yet. `count` names of `kind` written
     * to `names`. GL_INVALID_VALUE for a negative count or no names to write
     * to, GL_OUT_OF_MEMORY once the table is full - and nothing is written
     * unless every one of them fits, because a partly filled array is one an
     * application would read whole. */
    uint32_t create(uint32_t kind, int32_t count, uint32_t *names);
    /* glImportMemoryWin32HandleEXT and glImportSemaphoreWin32HandleEXT: the
     * handle resolved to the object behind it, opened on the device, and the
     * name it answered remembered against `name`. GL_INVALID_VALUE for a name
     * this context did not create or one already imported, GL_INVALID_OPERATION
     * for a handle type this path does not take and for a handle from nowhere,
     * GL_OUT_OF_MEMORY when the device would not open it. `size` is ignored
     * for a semaphore, which has none. */
    uint32_t import(ClientTransport &wire, uint32_t kind, uint32_t name, uint64_t size, uint32_t handle_type,
                    void *handle);
    /* glDeleteMemoryObjectsEXT and glDeleteSemaphoresEXT: each name's object
     * released on the device and the row dropped. A name this context did not
     * create is ignored, as GL ignores a name it never made; a negative count
     * is GL_INVALID_VALUE. */
    uint32_t destroy(ClientTransport &wire, uint32_t kind, int32_t count, const uint32_t *names);
    /* The host's own name for one this table handed out and imported, or 0 -
     * which is what every entry point naming an object substitutes, and what
     * makes a name the application invented reach no object at all. */
    uint32_t made(uint32_t kind, uint32_t name) const;
    /* glIsMemoryObjectEXT and glIsSemaphoreEXT: whether this context created
     * `name` in `kind`'s namespace, imported or not. The extension says a
     * created name is an object, and the host has none for it until the
     * import, so this is what answers rather than the host. */
    bool     holds(uint32_t kind, uint32_t name) const;
    /* glMemoryObjectParameterivEXT and glSemaphoreParameterui64vEXT, and the
     * two queries beside them. A parameter is recorded rather than sent: it is
     * set before the import, which is where the host object is made, so there
     * is nothing on the host to set it on - and `import` is what refuses one
     * this path cannot keep (research.md §12).
     *
     * GL_INVALID_VALUE for a name this context did not create, for a kind that
     * is not one, and for a query with nowhere to answer; GL_INVALID_ENUM for
     * a pname the object's kind does not have. */
    uint32_t set_parameter(uint32_t kind, uint32_t name, uint32_t pname, int32_t value);
    uint32_t get_parameter(uint32_t kind, uint32_t name, uint32_t pname, int32_t *value) const;

    /* Every object still held, released: what a context torn down gives back,
     * since the device counts this context as a holder of each. */
    void     release_all(ClientTransport &wire);
    uint32_t live() const;
    void     report(StateWriter &writer) const;

    /* A row is empty or holds one non-zero name of one namespace, no name
     * appears twice in one namespace, and an imported row has both a host name
     * and an object id while an unimported one has neither. */
    bool invariants_hold() const;

private:
    friend struct GlExternalObjectsProbe;

    /* Whether `kind` is one of the three the wire has, and which namespace it
     * names. */
    bool                     known(uint32_t kind, bool &memory) const;
    GlSharedObjectRow       *row_of(bool memory, uint32_t name);
    const GlSharedObjectRow *row_of(bool memory, uint32_t name) const;
    /* The object of one row given back to the device, and the row emptied. */
    void                     give_back(ClientTransport &wire, GlSharedObjectRow &row);

    icd::SharedHandles &handles_;
    icd::LogSink       &log_;
    GlSharedObjectRow   rows_[max_objects];
    uint32_t            live_;
    /* The next name this ICD hands out. Its own counter, because the only
     * memory objects and semaphores that exist in this context are the ones it
     * made - the host's names for them are never seen by the application. */
    uint32_t            next_name_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GL_EXTERNAL_OBJECTS_HPP
