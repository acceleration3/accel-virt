#include "executor/d3d/d3d12_heap_flags.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t shared = 0x1u;
constexpr uint32_t deny_buffers = 0x4u;
constexpr uint32_t shared_cross_adapter = 0x20u;
constexpr uint32_t hardware_protected = 0x100u;
constexpr uint32_t allow_display = 0x8u;
constexpr uint32_t deny_render_target_depth_stencil = 0x40u;
constexpr uint32_t deny_other_textures = 0x80u;
constexpr uint32_t whitelist = shared | deny_buffers | allow_display | shared_cross_adapter |
                           deny_render_target_depth_stencil | deny_other_textures;

}

D3d12HeapFlags::D3d12HeapFlags(uint32_t guest) : guest_(guest)
{
    AV_SCOPE("D3d12HeapFlags::D3d12HeapFlags");
}

bool D3d12HeapFlags::refused() const
{
    AV_SCOPE("D3d12HeapFlags::refused");
    return (guest_ & hardware_protected) != 0u;
}

uint32_t D3d12HeapFlags::kept() const
{
    AV_SCOPE("D3d12HeapFlags::kept");
    return guest_ & whitelist;
}

}
