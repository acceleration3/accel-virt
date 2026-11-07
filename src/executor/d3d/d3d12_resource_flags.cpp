#include "executor/d3d/d3d12_resource_flags.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t allow_render_target = 0x1u;
constexpr uint32_t allow_depth_stencil = 0x2u;
constexpr uint32_t allow_unordered_access = 0x4u;
constexpr uint32_t deny_shader_resource = 0x8u;
constexpr uint32_t allow_simultaneous_access = 0x20u;
constexpr uint32_t whitelist =
    allow_render_target | allow_depth_stencil | allow_unordered_access | deny_shader_resource | allow_simultaneous_access;

}

D3d12ResourceFlags::D3d12ResourceFlags(uint32_t guest) : guest_(guest)
{
    AV_SCOPE("D3d12ResourceFlags::D3d12ResourceFlags");
}

uint32_t D3d12ResourceFlags::kept() const
{
    AV_SCOPE("D3d12ResourceFlags::kept");
    return guest_ & whitelist;
}

}
