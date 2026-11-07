#ifndef AV_D3D_OBJECT_TABLE_HPP
#define AV_D3D_OBJECT_TABLE_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/host_object_registry.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {
class BlockSource;
struct D3dObjectTableProbe;
}

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
    heap12,
    resource12,
    descriptor_heap12,
    root_signature12,
    pipeline12,
    queue12,
    allocator12,
    list12,
    query_heap12,
    command_signature12,
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
    static constexpr uint32_t no_slot = 0xFFFFFFFFu;

    explicit D3dObjectTable(BlockSource &blocks);
    ~D3dObjectTable();
    D3dObjectTable(const D3dObjectTable &) = delete;
    D3dObjectTable &operator=(const D3dObjectTable &) = delete;
    D3dObjectTable(D3dObjectTable &&) = delete;
    D3dObjectTable &operator=(D3dObjectTable &&) = delete;

    [[nodiscard]] Status insert(uint32_t id, D3dObjectKind kind, IUnknown *object);
    [[nodiscard]] Status keep(uint32_t id, D3dObjectKind kind, IUnknown *made);
    IUnknown          *find(uint32_t id, D3dObjectKind kind) const;
    template <typename T>
    T                 *find_as(uint32_t id, D3dObjectKind kind) const;
    IUnknown          *find_any(uint32_t id, D3dObjectKind &kind) const;
    IUnknown          *find_view(uint32_t id) const;
    IUnknown          *exchange(uint32_t id, D3dObjectKind kind, IUnknown *object);
    uint32_t           next_used(uint32_t from) const;
    uint32_t           views() const;
    uint32_t           view_slot(uint32_t index) const;
    IUnknown          *viewed(uint32_t slot) const;
    void               note_viewed(uint32_t slot, IUnknown *resource);
    IUnknown          *at(uint32_t slot, uint32_t &id, D3dObjectKind &kind) const;
    void               remove(uint32_t id);
    IUnknown          *take(uint32_t id);
    void               remove_kinds(D3dObjectKind first, D3dObjectKind last);
    uint32_t           count_kinds(D3dObjectKind first, D3dObjectKind last) const;
    void               release_all();
    uint32_t           count() const;

    bool invariants_hold() const;

private:
    struct Entry {
        IUnknown *object_;
        IUnknown *viewed_;
        uint32_t  view_at_;
    };

    void let_go(D3dObjectKind kind, const Entry &entry);
    void unlist(D3dObjectKind kind, const Entry &entry);

    HostObjectRegistry<uint32_t, D3dObjectKind, Entry> objects_;
    GrowableArray<uint32_t>                            views_;

    friend struct D3dObjectTableProbe;
};

template <typename T>
T *D3dObjectTable::find_as(uint32_t id, D3dObjectKind kind) const
{
    AV_SCOPE("D3dObjectTable::find_as");
    return static_cast<T *>(static_cast<void *>(find(id, kind)));
}

}

#endif
