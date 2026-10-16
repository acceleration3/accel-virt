/* Every Direct3D object one session made, by the id the guest named it with.
 *
 * The guest chooses the ids - a user-mode driver has to name an object in the
 * same batch that creates it, long before any answer could come back - so the
 * table is keyed by the guest's number and the host accepts a number only
 * where it made an object of the kind the command wants. An id naming
 * nothing, or naming a sampler where a view belongs, is a refused command,
 * never a pointer handed to DXVK. Each entry holds one reference to its
 * object.
 *
 * There is no count past which an object is refused: Resident Evil 4 held
 * 196608 a minute into its first area. The entries are KeyedSlots, so an
 * object keeps its slot for as long as it is kept - what a swap chain's
 * rotation lists the views by, and remembers what each view sees by - and
 * the views are listed apart, so a rotation walks those alone. The table
 * holds memory for what it holds, and a clear gives it all back. */
#ifndef AV_D3D_OBJECT_TABLE_HPP
#define AV_D3D_OBJECT_TABLE_HPP

#include "core/support/growable_array.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {
class BlockSource;
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
    /* What a slot query answers for no slot. */
    static constexpr uint32_t no_slot = 0xFFFFFFFFu;

    explicit D3dObjectTable(BlockSource &blocks);
    /* Gives the memory back. The objects are not released: a session clears
     * its table while its device is alive. */
    ~D3dObjectTable();
    D3dObjectTable(const D3dObjectTable &) = delete;
    D3dObjectTable &operator=(const D3dObjectTable &) = delete;
    D3dObjectTable(D3dObjectTable &&) = delete;
    D3dObjectTable &operator=(D3dObjectTable &&) = delete;

    /* Takes the caller's reference to `object` under `id`: invalid_parameter
     * for id 0, a kind of none or no object, conflict for an id in use,
     * no_memory when the table cannot grow. The reference stays the caller's
     * unless ok. */
    [[nodiscard]] Status insert(uint32_t id, D3dObjectKind kind, IUnknown *object);
    /* `made`, just made, kept under `id`: the reference it came with is the
     * table's, or released when it cannot be kept, with insert's answer.
     * invalid_parameter for a null `made`, which a refused create leaves. */
    [[nodiscard]] Status keep(uint32_t id, D3dObjectKind kind, IUnknown *made);
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
    /* The first slot at or after `from` holding an object; no_slot for none. */
    uint32_t           next_used(uint32_t from) const;
    /* The slots holding views, so a swap chain's rotation looks at those alone. */
    uint32_t           views() const;
    /* The `index`th view's slot; no_slot past the last. */
    uint32_t           view_slot(uint32_t index) const;
    /* The resource a view slot was last noted to see; null until noted, and
     * for a slot holding nothing. */
    IUnknown          *viewed(uint32_t slot) const;
    void               note_viewed(uint32_t slot, IUnknown *resource);
    /* The object in `slot` and its id and kind; null for a slot holding nothing. */
    IUnknown          *at(uint32_t slot, uint32_t &id, D3dObjectKind &kind) const;
    /* Removes `id` and gives its reference back; nothing for an unknown id. */
    void               remove(uint32_t id);
    /* Every object of a kind from `first` to `last` removed, its reference
     * given back: one API's objects as its session goes, the others' kept. */
    void               remove_kinds(D3dObjectKind first, D3dObjectKind last);
    /* How many objects of a kind from `first` to `last` are kept. */
    uint32_t           count_kinds(D3dObjectKind first, D3dObjectKind last) const;
    /* Every reference back, and the table's memory to the system. */
    void               clear();
    uint32_t           count() const;

    bool invariants_hold() const;

private:
    struct Entry {
        IUnknown     *object_;
        /* What a view sees, once asked: a view holds its resource, so the
         * pointer lives as long as the view. Cleared when the entry changes. */
        IUnknown     *viewed_;
        /* Where a view's slot is in views_. */
        uint32_t      view_at_;
        D3dObjectKind kind_;
    };

    /* The slot's entry gone from the views, if it was one, and from the table. */
    void forget(uint32_t slot, const Entry &entry);

    KeyedSlots<uint32_t, Entry> objects_;
    GrowableArray<uint32_t>     views_;

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
