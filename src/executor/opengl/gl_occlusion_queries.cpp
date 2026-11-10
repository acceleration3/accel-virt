#include "executor/opengl/gl_occlusion_queries.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"

namespace av {

namespace {

constexpr uint64_t gl_samples_passed = 0x8914u;
constexpr uint64_t gl_any_samples_passed = 0x8C2Fu;
constexpr uint64_t gl_any_samples_passed_conservative = 0x8D6Au;

}

GlOcclusionQueries::GlOcclusionQueries() : active_{}
{
    AV_SCOPE("GlOcclusionQueries::GlOcclusionQueries");
}

bool GlOcclusionQueries::occlusion(uint64_t target, uint32_t &slot) const
{
    AV_SCOPE("GlOcclusionQueries::occlusion");
    if (target == gl_samples_passed) slot = 0u;
    else if (target == gl_any_samples_passed) slot = 1u;
    else if (target == gl_any_samples_passed_conservative) slot = 2u;
    else return false;
    return true;
}

void GlOcclusionQueries::ran(uint32_t command_id, WireReader payload)
{
    AV_SCOPE("GlOcclusionQueries::ran");
    const bool begin = command_id == glproto::kCmd_glBeginQuery || command_id == glproto::kCmd_glBeginQueryIndexed;
    const bool end = command_id == glproto::kCmd_glEndQuery || command_id == glproto::kCmd_glEndQueryIndexed;
    if (!begin && !end) return;
    uint64_t target = 0u;
    uint32_t slot = 0u;
    if (!payload.read_varint(target) || !occlusion(target, slot)) return;
    active_[slot] = begin ? 1u : 0u;
}

bool GlOcclusionQueries::open() const
{
    AV_SCOPE("GlOcclusionQueries::open");
    for (const uint32_t active : active_)
        if (active != 0u) return true;
    return false;
}

void GlOcclusionQueries::clear()
{
    AV_SCOPE("GlOcclusionQueries::clear");
    for (uint32_t &active : active_) active = 0u;
}

bool GlOcclusionQueries::invariants_hold() const
{
    AV_SCOPE("GlOcclusionQueries::invariants_hold");
    for (const uint32_t active : active_)
        if (active > 1u) return false;
    return true;
}

}
