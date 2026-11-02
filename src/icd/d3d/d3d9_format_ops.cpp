#include "icd/d3d/d3d9_format_ops.hpp"

#include "core/support/four_cc.hpp"
#include "core/trace/function_scope.hpp"

namespace av::d3d {

namespace {
constexpr uint32_t a8r8g8b8 = 21u;
constexpr uint32_t x8r8g8b8 = 22u;
constexpr uint32_t r5g6b5 = 23u;
constexpr uint32_t x1r5g5b5 = 24u;
constexpr uint32_t a1r5g5b5 = 25u;
constexpr uint32_t a4r4g4b4 = 26u;
constexpr uint32_t a2r10g10b10 = 35u;

constexpr uint32_t asked_formats[] = {
    20u, a8r8g8b8, x8r8g8b8, r5g6b5, x1r5g5b5, a1r5g5b5, a4r4g4b4, 27u, 28u, 29u, 30u, 31u, 32u, 33u, 34u,
    a2r10g10b10, 36u, 40u, 41u, 50u, 51u, 52u, 60u, 61u, 62u, 63u, 64u, 67u, 70u, 71u, 73u, 75u, 77u, 79u, 80u,
    81u, 82u, 83u, 110u, 111u, 112u, 113u, 114u, 115u, 116u, 117u,
    FourCc('D', 'X', 'T', '1').value(), FourCc('D', 'X', 'T', '2').value(), FourCc('D', 'X', 'T', '3').value(), FourCc('D', 'X', 'T', '4').value(),
    FourCc('D', 'X', 'T', '5').value(), FourCc('A', 'T', 'I', '1').value(), FourCc('A', 'T', 'I', '2').value(), FourCc('I', 'N', 'T', 'Z').value(),
    FourCc('D', 'F', '1', '6').value(), FourCc('D', 'F', '2', '4').value(), FourCc('N', 'U', 'L', 'L').value(), FourCc('R', 'E', 'S', 'Z').value(),
    FourCc('U', 'Y', 'V', 'Y').value(), FourCc('Y', 'U', 'Y', '2').value(), FourCc('R', 'G', 'B', 'G').value(), FourCc('G', 'R', 'G', 'B').value()};
constexpr uint32_t asked_count = sizeof(asked_formats) / sizeof(asked_formats[0]);
static_assert(asked_count <= D3d9FormatOps::max_formats, "every format asked fits the table");

constexpr uint32_t can_texture = 1u << 0u;
constexpr uint32_t can_cube = 1u << 1u;
constexpr uint32_t can_volume = 1u << 2u;
constexpr uint32_t can_rt_texture = 1u << 3u;
constexpr uint32_t can_rt_surface = 1u << 4u;
constexpr uint32_t can_depth = 1u << 5u;
constexpr uint32_t can_vertex_texture = 1u << 6u;
constexpr uint32_t can_srgb_read = 1u << 7u;
constexpr uint32_t can_srgb_write = 1u << 8u;
constexpr uint32_t can_filter = 1u << 9u;
constexpr uint32_t can_blend = 1u << 10u;
constexpr uint32_t can_autogen = 1u << 11u;
constexpr uint32_t can_back_buffer = 1u << 12u;
constexpr uint32_t can_offscreen = 1u << 13u;
constexpr uint32_t can_bump = 1u << 14u;
constexpr uint32_t can_depth_texture = 1u << 15u;

constexpr uint32_t op_texture = 0x1u;
constexpr uint32_t op_volume = 0x2u;
constexpr uint32_t op_cube = 0x4u;
constexpr uint32_t op_offscreen_rt = 0x8u;
constexpr uint32_t op_same_format_rt = 0x10u;
constexpr uint32_t op_zstencil = 0x40u;
constexpr uint32_t op_zstencil_any_depth = 0x80u;
constexpr uint32_t op_up_to_alpha_rt = 0x100u;
constexpr uint32_t op_display = 0x400u;
constexpr uint32_t op_3d = 0x800u;
constexpr uint32_t op_offscreen_plain = 0x4000u;
constexpr uint32_t op_srgb_read = 0x8000u;
constexpr uint32_t op_bump = 0x10000u;
constexpr uint32_t op_no_filter = 0x40000u;
constexpr uint32_t op_member_argb = 0x80000u;
constexpr uint32_t op_srgb_write = 0x100000u;
constexpr uint32_t op_no_blend = 0x200000u;
constexpr uint32_t op_autogen = 0x400000u;
constexpr uint32_t op_vertex_texture = 0x800000u;

struct Grant {
    uint32_t support;
    uint32_t operation;
};
constexpr Grant grants[] = {
    {can_texture, op_texture},
    {can_cube, op_cube},
    {can_volume, op_volume},
    {can_rt_texture, op_offscreen_rt | op_same_format_rt},
    {can_rt_surface, op_offscreen_rt | op_same_format_rt},
    {can_depth, op_zstencil | op_zstencil_any_depth},
    {can_depth_texture, op_texture},
    {can_vertex_texture, op_vertex_texture},
    {can_srgb_read, op_srgb_read},
    {can_srgb_write, op_srgb_write},
    {can_autogen, op_autogen},
    {can_offscreen, op_offscreen_plain},
    {can_bump, op_bump},
};

}

D3d9FormatOps::D3d9FormatOps() : offered_{}, count_(0u)
{
    AV_SCOPE("D3d9FormatOps::D3d9FormatOps");
}

uint32_t D3d9FormatOps::formats_to_ask() const
{
    AV_SCOPE("D3d9FormatOps::formats_to_ask");
    return asked_count;
}

uint32_t D3d9FormatOps::format_to_ask(uint32_t index) const
{
    AV_SCOPE("D3d9FormatOps::format_to_ask");
    return index < asked_count ? asked_formats[index] : 0u;
}

bool D3d9FormatOps::asked(uint32_t format) const
{
    AV_SCOPE("D3d9FormatOps::asked");
    for (const uint32_t candidate : asked_formats)
        if (candidate == format) return true;
    return false;
}

uint32_t D3d9FormatOps::operations(uint32_t format, uint32_t support) const
{
    AV_SCOPE("D3d9FormatOps::operations");
    uint32_t granted = 0u;
    for (const Grant &grant : grants)
        if ((support & grant.support) != 0u) granted |= grant.operation;
    const bool renders = (support & (can_rt_texture | can_rt_surface)) != 0u;
    if ((support & can_back_buffer) != 0u && (format == x8r8g8b8 || format == r5g6b5))
        granted |= op_display | op_3d;
    if (renders && (format == a8r8g8b8 || format == a1r5g5b5 || format == a4r4g4b4 || format == a2r10g10b10))
        granted |= op_up_to_alpha_rt;
    if (renders && (format == a8r8g8b8 || format == x8r8g8b8 || format == a2r10g10b10 || format == r5g6b5 ||
                    format == x1r5g5b5 || format == a1r5g5b5))
        granted |= op_member_argb;
    if ((granted & op_texture) != 0u && (support & can_filter) == 0u && (support & can_depth) == 0u)
        granted |= op_no_filter;
    if (renders && (support & can_blend) == 0u) granted |= op_no_blend;
    return granted;
}

void D3d9FormatOps::record(uint32_t format, uint32_t support, uint32_t multisample)
{
    AV_SCOPE("D3d9FormatOps::record");
    if (!asked(format)) return;
    const uint32_t granted = operations(format, support);
    const uint32_t types = multisample >> 1u;
    const uint32_t samples = types != 0u ? types | 1u : 0u;
    for (uint32_t index = 0u; index < count_; index++) {
        if (offered_[index].format_ != format) continue;
        if (granted != 0u) {
            offered_[index] = Op{format, granted, samples};
            return;
        }
        offered_[index] = offered_[count_ - 1u];
        count_--;
        return;
    }
    if (granted != 0u) offered_[count_++] = Op{format, granted, samples};
}

uint32_t D3d9FormatOps::count() const
{
    AV_SCOPE("D3d9FormatOps::count");
    return count_;
}

const D3d9FormatOps::Op &D3d9FormatOps::at(uint32_t index) const
{
    AV_SCOPE("D3d9FormatOps::at");
    return offered_[index];
}

bool D3d9FormatOps::invariants_hold() const
{
    AV_SCOPE("D3d9FormatOps::invariants_hold");
    if (count_ > max_formats) return false;
    for (uint32_t one = 0u; one < count_; one++) {
        if (offered_[one].operations_ == 0u || !asked(offered_[one].format_)) return false;
        for (uint32_t other = one + 1u; other < count_; other++)
            if (offered_[other].format_ == offered_[one].format_) return false;
    }
    return true;
}

}
