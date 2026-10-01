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
 * than served. Each entry holds one reference to its object. */
#ifndef AV_D3D_OBJECT_TABLE_HPP
#define AV_D3D_OBJECT_TABLE_HPP

#include <stdint.h>

struct IUnknown;

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
    static constexpr uint32_t capacity = 32768u;

    D3dObjectTable();
    D3dObjectTable(const D3dObjectTable &) = delete;
    D3dObjectTable &operator=(const D3dObjectTable &) = delete;

    /* Takes the caller's reference to `object` under `id`. False, with the
     * reference still the caller's, for id 0, an id in use and a full table. */
    [[nodiscard]] bool insert(uint32_t id, D3dObjectKind kind, IUnknown *object);
    /* The object `id` names if it is of `kind`; null otherwise. No reference
     * is taken: the table's lasts until the id is removed. */
    IUnknown          *find(uint32_t id, D3dObjectKind kind) const;
    /* The object `id` names and its kind, whatever it is. */
    IUnknown          *find_any(uint32_t id, D3dObjectKind &kind) const;
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
    /* Every reference back. */
    void               clear();
    uint32_t           count() const;

    bool invariants_hold() const;

private:
    uint32_t slot_of(uint32_t id) const;
    uint32_t home_of(uint32_t id) const;
    void     set(uint32_t slot, uint32_t id, D3dObjectKind kind, bool tombstone, IUnknown *object);

    /* One slot across the four: the guest's id, what it names, whether a
     * removal left the slot (so a probe carries on past it), and the object. */
    uint32_t      ids_[capacity];
    D3dObjectKind kinds_[capacity];
    bool          tombstones_[capacity];
    IUnknown     *objects_[capacity];
    /* What a view slot's view sees, once asked: a view holds its resource,
     * so the pointer lives as long as the view. Cleared when the slot changes. */
    IUnknown     *viewed_[capacity];
    uint32_t      view_slots_[capacity];
    uint32_t      view_at_[capacity];
    uint32_t      views_;
    uint32_t      count_;
};

}  // namespace av

#endif  // AV_D3D_OBJECT_TABLE_HPP
