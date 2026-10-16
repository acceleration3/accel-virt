/* Every Direct3D object one session made, by the id the guest named it with.
 *
 * The guest chooses the ids - a user-mode driver has to name an object in the
 * same batch that creates it, long before any answer could come back - so the
 * table is keyed by the guest's number and the host accepts a number only
 * where it made an object of the kind the command wants. An id naming
 * nothing, or naming a sampler where a view belongs, is a refused command,
 * never a pointer handed to DXVK.
 *
 * Open addressing over a fixed table: a desktop compositor holds a few
 * thousand objects, and a guest that wants more than this is refused rather
 * than served. Each entry holds one reference to its object.
 *
 * Resident Evil 4 held 24576 objects - the old table's three quarters - a
 * minute into loading its first area, and every create after that was
 * refused. The table is mapped at its first insert and its pages are given
 * back when it is cleared, so a session holding a few objects costs the few
 * pages they touched, not the eight megabytes of a full table; the executor
 * keeps one per session. */
#ifndef AV_D3D_OBJECT_TABLE_HPP
#define AV_D3D_OBJECT_TABLE_HPP

#include "core/trace/function_scope.hpp"

#include <stddef.h>
#include <stdint.h>

struct IUnknown;

namespace av {
struct D3dObjectTableProbe;
}  // namespace av

namespace av {

enum class D3dObjectKind : uint8_t {
    none,
    resource,
    shader_resource_view,
    render_target_view,
    depth_stencil_view,
    unordered_access_view,
    vertex_shader,
    hull_shader,
    domain_shader,
    geometry_shader,
    pixel_shader,
    compute_shader,
    input_layout,
    blend_state,
    depth_stencil_state,
    rasterizer_state,
    sampler,
    query,
    /* Direct3D 12's, in the same session's table under ids of their own. */
    heap12,
    resource12,
    descriptor_heap12,
    root_signature12,
    pipeline12,
    queue12,
    allocator12,
    list12,
    fence12,
    query_heap12,
    command_signature12,
    /* Direct3D 9's, last: the session's table is shared, and a Direct3D 9
     * session shutting down lets go of these kinds alone. */
    surface9,
    texture9,
    cube_texture9,
    volume_texture9,
    vertex_buffer9,
    index_buffer9,
    vertex_shader9,
    pixel_shader9,
    vertex_declaration9,
    query9
};

class D3dObjectTable {
public:
    static constexpr uint32_t capacity = 262144u;

    D3dObjectTable();
    /* Gives the mapping back. The objects are not released: a session clears
     * its table while its device is alive. */
    ~D3dObjectTable();
    D3dObjectTable(const D3dObjectTable &) = delete;
    D3dObjectTable &operator=(const D3dObjectTable &) = delete;
    D3dObjectTable(D3dObjectTable &&) = delete;
    D3dObjectTable &operator=(D3dObjectTable &&) = delete;

    /* Takes the caller's reference to `object` under `id`. False, with the
     * reference still the caller's, for id 0, an id in use, a full table and
     * a table no memory could be mapped for. */
    [[nodiscard]] bool insert(uint32_t id, D3dObjectKind kind, IUnknown *object);
    /* `made`, just made, kept under `id`: the reference it came with is the
     * table's, or released when it cannot be kept - an id already taken, a
     * full table. False then, and for a null `made`, which a refused create
     * leaves. */
    [[nodiscard]] bool keep(uint32_t id, D3dObjectKind kind, IUnknown *made);
    /* The object `id` names if it is of `kind`; null otherwise. No reference
     * is taken: the table's lasts until the id is removed. */
    IUnknown          *find(uint32_t id, D3dObjectKind kind) const;
    /* The same, as the interface `kind` is kept as. */
    template <typename T>
    T                 *find_as(uint32_t id, D3dObjectKind kind) const;
    /* The object `id` names and its kind, whatever it is. */
    IUnknown          *find_any(uint32_t id, D3dObjectKind &kind) const;
    /* The view `id` names, of whichever of the four view kinds; null for
     * anything that is not a view. */
    IUnknown          *find_view(uint32_t id) const;
    /* Puts `object` under `id` in place of what it named, if that was of
     * `kind`, and hands the old reference back; null, with `object` still the
     * caller's, otherwise. */
    IUnknown          *exchange(uint32_t id, D3dObjectKind kind, IUnknown *object);
    /* The object in `slot` and its id and kind; null for an empty slot. */
    /* The first slot at or after `from` holding an object; capacity for none. */
    uint32_t           next_used(uint32_t from) const;
    /* The slots holding views, so a swap chain's rotation looks at those alone. */
    uint32_t           views() const;
    uint32_t           view_slot(uint32_t index) const;
    /* The resource a view slot was last noted to see; null until noted. */
    IUnknown          *viewed(uint32_t slot) const;
    void               note_viewed(uint32_t slot, IUnknown *resource);
    IUnknown          *at(uint32_t slot, uint32_t &id, D3dObjectKind &kind) const;
    /* Removes `id` and gives its reference back; nothing for an unknown id. */
    void               remove(uint32_t id);
    /* Every object of a kind from `first` to `last` removed, its reference
     * given back: one API's objects as its session goes, the others' kept. */
    void               remove_kinds(D3dObjectKind first, D3dObjectKind last);
    /* How many objects of a kind from `first` to `last` are kept. */
    uint32_t           count_kinds(D3dObjectKind first, D3dObjectKind last) const;
    /* Every reference back, and the table's pages to the system. */
    void               clear();
    uint32_t           count() const;

    bool invariants_hold() const;

private:
    static constexpr size_t table_bytes =
        size_t{capacity} * (2u * sizeof(IUnknown *) + 3u * sizeof(uint32_t) + sizeof(D3dObjectKind) + sizeof(bool));

    uint32_t slot_of(uint32_t id) const;
    uint32_t home_of(uint32_t id) const;
    void     set(uint32_t slot, uint32_t id, D3dObjectKind kind, bool tombstone, IUnknown *object);
    /* The table's memory, mapped once: false when none could be. */
    bool     reserve();

    void          *table_;
    /* One slot across the arrays: the guest's id, what it names, whether a
     * removal left the slot (so a probe carries on past it), and the object;
     * null until the table is mapped. */
    IUnknown     **objects_;
    /* What a view slot's view sees, once asked: a view holds its resource,
     * so the pointer lives as long as the view. Cleared when the slot changes. */
    IUnknown     **viewed_;
    uint32_t      *ids_;
    uint32_t      *view_slots_;
    uint32_t      *view_at_;
    D3dObjectKind *kinds_;
    bool          *tombstones_;
    uint32_t       views_;
    uint32_t       count_;

    friend struct D3dObjectTableProbe;
};

template <typename T>
T *D3dObjectTable::find_as(uint32_t id, D3dObjectKind kind) const
{
    AV_SCOPE("D3dObjectTable::find_as");
    return static_cast<T *>(static_cast<void *>(find(id, kind)));
}

}  // namespace av

#endif  // AV_D3D_OBJECT_TABLE_HPP
