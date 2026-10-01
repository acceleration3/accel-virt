/* The OpenGL memory objects and semaphores one guest context imported from a
 * shared descriptor (feature 053).
 *
 * GL names them with small integers the driver chooses, in two name spaces
 * that do not share: memory object 3 and semaphore 3 are different objects.
 * So a row is a name and which of the two it is, and a release that named the
 * other one finds nothing - a guest that could cross them would have
 * glDeleteSemaphoresEXT delete somebody's memory object. A name this context
 * did not import finds nothing either, which is what stops a guest naming a
 * number it chose.
 *
 * A WDDM monitored fence and a native fence are one host semaphore, so a row
 * made as either is found by either: the two kinds differ to the guest and not
 * to the host, and refusing the other would leak the object. */
#ifndef AV_GL_SHARED_OBJECTS_HPP
#define AV_GL_SHARED_OBJECTS_HPP

#include <stdint.h>

namespace av {

class GlSharedObjects {
public:
    /* One guest process's shared textures and fences, which is a handful, with
     * the room the device's own per-context table has. */
    static constexpr uint32_t max_objects = 64u;

    GlSharedObjects();
    GlSharedObjects(const GlSharedObjects &) = delete;
    GlSharedObjects &operator=(const GlSharedObjects &) = delete;

    /* Remembers `name` as an object of `kind` this context imported. False for
     * a name of 0, a kind this protocol does not have, a table that is full,
     * and a name this context already holds in that name space - two rows for
     * one GL name would delete it twice. */
    [[nodiscard]] bool remember(uint32_t kind, uint32_t name);
    /* The row `wire_handle` names in `kind`'s name space, dropped, with the GL
     * name in `name`. False, and `name` untouched, for a handle this context
     * did not import - including one no GL name could be. */
    [[nodiscard]] bool forget(uint32_t kind, uint64_t wire_handle, uint32_t &name);
    /* One row still held, dropped: what a teardown deletes them all through.
     * `kind` is AV_SHARED_MEMORY or AV_SHARED_NATIVE_FENCE, which is which
     * entry point deletes it. */
    [[nodiscard]] bool take_any(uint32_t &kind, uint32_t &name);

    uint32_t count() const;

    bool invariants_hold() const;

private:
    friend struct GlSharedObjectsProbe;

    /* Which name space a guest's kind is, as a row records it. 0 for a kind
     * this protocol does not have. */
    uint32_t space_of(uint32_t kind) const;

    uint32_t kinds_[max_objects];
    uint32_t names_[max_objects];
    uint32_t count_;
};

}  // namespace av

#endif  // AV_GL_SHARED_OBJECTS_HPP
