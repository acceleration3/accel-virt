#include "executor/opengl/gl_fence_deferral.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"

namespace av {

namespace {

constexpr uint64_t gl_blend = 0x0BE2u;
constexpr uint64_t first_advanced_equation = 0x9294u;
constexpr uint64_t last_advanced_equation = 0x92B0u;

bool advanced(uint64_t mode)
{
    AV_SCOPE("advanced");
    return mode >= first_advanced_equation && mode <= last_advanced_equation;
}

}

GlFenceDeferral::GlFenceDeferral() : occlusion_(), blending_{}, advanced_{}, owed_(0u)
{
    AV_SCOPE("GlFenceDeferral::GlFenceDeferral");
}

void GlFenceDeferral::blend_enable(uint64_t cap, bool on, uint64_t index, bool indexed)
{
    AV_SCOPE("GlFenceDeferral::blend_enable");
    if (cap != gl_blend) return;
    if (indexed) {
        if (index < buffers) blending_[index] = on;
        return;
    }
    for (bool &flag : blending_) flag = on;
}

void GlFenceDeferral::blend_equation(uint64_t buffer, uint64_t mode, bool indexed)
{
    AV_SCOPE("GlFenceDeferral::blend_equation");
    if (indexed) {
        if (buffer < buffers) advanced_[buffer] = advanced(mode);
        return;
    }
    for (bool &flag : advanced_) flag = advanced(mode);
}

void GlFenceDeferral::ran(uint32_t command_id, WireReader payload)
{
    AV_SCOPE("GlFenceDeferral::ran");
    occlusion_.ran(command_id, payload);
    uint64_t first = 0u, second = 0u, third = 0u;
    switch (command_id) {
    case glproto::kCmd_glEnable:
    case glproto::kCmd_glDisable:
        if (payload.read_varint(first)) blend_enable(first, command_id == glproto::kCmd_glEnable, 0u, false);
        break;
    case glproto::kCmd_glEnablei:
    case glproto::kCmd_glDisablei:
        if (payload.read_varint(first) && payload.read_varint(second))
            blend_enable(first, command_id == glproto::kCmd_glEnablei, second, true);
        break;
    case glproto::kCmd_glBlendEquation:
        if (payload.read_varint(first)) blend_equation(0u, first, false);
        break;
    case glproto::kCmd_glBlendEquationi:
        if (payload.read_varint(first) && payload.read_varint(second)) blend_equation(first, second, true);
        break;
    case glproto::kCmd_glBlendEquationSeparate:
        if (payload.read_varint(first) && payload.read_varint(second)) blend_equation(0u, first, false);
        break;
    case glproto::kCmd_glBlendEquationSeparatei:
        if (payload.read_varint(first) && payload.read_varint(second) && payload.read_varint(third))
            blend_equation(first, second, true);
        break;
    default: break;
    }
}

bool GlFenceDeferral::occlusion_open() const
{
    AV_SCOPE("GlFenceDeferral::occlusion_open");
    return occlusion_.open();
}

bool GlFenceDeferral::advanced_blending() const
{
    AV_SCOPE("GlFenceDeferral::advanced_blending");
    for (uint32_t at = 0u; at < buffers; at++)
        if (blending_[at] && advanced_[at]) return true;
    return false;
}

bool GlFenceDeferral::defers() const
{
    AV_SCOPE("GlFenceDeferral::defers");
    return owed_ < most_owed && (occlusion_.open() || advanced_blending());
}

bool GlFenceDeferral::demands(uint32_t command_id) const
{
    AV_SCOPE("GlFenceDeferral::demands");
    switch (command_id) {
    case glproto::kCmd_glFinish:
    case glproto::kCmd_glFlush:
    case glproto::kCmd_glClientWaitSync:
    case glproto::kCmd_glWaitSync:
    case glproto::kCmd_glFenceSync:
    case glproto::kCmd_glSignalSemaphoreEXT:
    case glproto::kCmd_glReadPixels:
    case glproto::kCmd_glReadnPixels:
    case glproto::kCmd_glGetTextureSubImage:
    case glproto::kCmd_glGetTexImage:
    case glproto::kCmd_glGetnTexImage:
    case glproto::kCmd_glGetCompressedTexImage:
    case glproto::kCmd_glGetBufferSubData:
    case glproto::kCmd_glGetNamedBufferSubData: return true;
    default: return false;
    }
}

void GlFenceDeferral::owe()
{
    AV_SCOPE("GlFenceDeferral::owe");
    if (owed_ < most_owed) owed_++;
}

void GlFenceDeferral::paid()
{
    AV_SCOPE("GlFenceDeferral::paid");
    owed_ = 0u;
}

uint32_t GlFenceDeferral::owed() const
{
    AV_SCOPE("GlFenceDeferral::owed");
    return owed_;
}

void GlFenceDeferral::clear()
{
    AV_SCOPE("GlFenceDeferral::clear");
    occlusion_.clear();
    for (bool &flag : blending_) flag = false;
    for (bool &flag : advanced_) flag = false;
    owed_ = 0u;
}

bool GlFenceDeferral::invariants_hold() const
{
    AV_SCOPE("GlFenceDeferral::invariants_hold");
    return owed_ <= most_owed && occlusion_.invariants_hold();
}

}
