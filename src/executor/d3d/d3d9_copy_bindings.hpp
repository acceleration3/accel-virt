#ifndef AV_D3D9_COPY_BINDINGS_HPP
#define AV_D3D9_COPY_BINDINGS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DResource9;

namespace av {

class BlockSource;

class D3d9CopyBindings {
public:
    static constexpr uint32_t most_streams = 16u;

    explicit D3d9CopyBindings(BlockSource &blocks);
    D3d9CopyBindings(const D3d9CopyBindings &) = delete;
    D3d9CopyBindings &operator=(const D3d9CopyBindings &) = delete;

    [[nodiscard]] Status add(uint32_t resource, IDirect3DResource9 *buffer, IDirect3DDevice9 *device);
    void                 drop(uint32_t resource);
    [[nodiscard]] Status stream_bound(IDirect3DDevice9 *device, uint32_t stream, uint32_t resource);
    [[nodiscard]] Status indices_bound(IDirect3DDevice9 *device, uint32_t resource);
    void                 forget_sources(IDirect3DDevice9 *device);
    template <typename Visitor>
    void                 drop_device(IDirect3DDevice9 *device, Visitor visit);
    template <typename Visitor>
    void                 drop_all(Visitor visit);
    template <typename Visitor>
    void                 due(IDirect3DDevice9 *device, Visitor visit);

    uint32_t count() const;

    bool invariants_hold() const;

private:
    struct Copy {
        IDirect3DResource9 *buffer_;
        IDirect3DDevice9   *device_;
        uint64_t            drawn_;
    };

    struct Sources {
        uint32_t streams_[most_streams];
        uint32_t indices_;
    };

    Sources *sources_of(IDirect3DDevice9 *device, Status &status);
    template <typename Visitor>
    void     due_one(IDirect3DDevice9 *device, uint32_t resource, Visitor &visit);

    KeyedSlots<uint32_t, Copy>       copies_;
    KeyedSlots<uintptr_t, Sources>   sources_;
    uint64_t                         draw_;
};

template <typename Visitor>
void D3d9CopyBindings::drop_device(IDirect3DDevice9 *device, Visitor visit)
{
    AV_SCOPE("D3d9CopyBindings::drop_device");
    copies_.erase_if([device, &visit](uint32_t resource, const Copy &held) {
        if (held.device_ != device) return false;
        visit(resource);
        return true;
    });
    forget_sources(device);
}

template <typename Visitor>
void D3d9CopyBindings::drop_all(Visitor visit)
{
    AV_SCOPE("D3d9CopyBindings::drop_all");
    copies_.erase_if([&visit](uint32_t resource, const Copy &) {
        visit(resource);
        return true;
    });
    sources_.clear();
}

template <typename Visitor>
void D3d9CopyBindings::due_one(IDirect3DDevice9 *device, uint32_t resource, Visitor &visit)
{
    AV_SCOPE("D3d9CopyBindings::due_one");
    if (resource == 0u) return;
    Copy *const held = copies_.find(resource);
    if (held == nullptr || held->device_ != device || held->drawn_ == draw_) return;
    held->drawn_ = draw_;
    visit(resource, held->buffer_);
}

template <typename Visitor>
void D3d9CopyBindings::due(IDirect3DDevice9 *device, Visitor visit)
{
    AV_SCOPE("D3d9CopyBindings::due");
    if (copies_.count() == 0u) return;
    const Sources *const bound = sources_.find(reinterpret_cast<uintptr_t>(device));
    if (bound == nullptr) return;
    draw_++;
    for (uint32_t stream = 0u; stream < most_streams; stream++) due_one(device, bound->streams_[stream], visit);
    due_one(device, bound->indices_, visit);
}

}

#endif
