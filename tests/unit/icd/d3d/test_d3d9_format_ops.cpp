#include "harness/framework/test_registry.hpp"
#include "icd/d3d/d3d9_format_ops.hpp"

#include <cstdint>

using av::d3d::D3d9FormatOps;
using Op = av::d3d::D3d9FormatOps::Op;

namespace av::d3d {

struct D3d9FormatOpsProbe {
    static void set_count(D3d9FormatOps &ops, std::uint32_t count) { ops.count_ = count; }
    static void set(D3d9FormatOps &ops, std::uint32_t index, std::uint32_t format, std::uint32_t operations)
    {
        ops.offered_[index] = D3d9FormatOps::Op{format, operations, 0u};
    }
};

}

using av::d3d::D3d9FormatOpsProbe;

namespace {
constexpr std::uint32_t texture = 1u << 0u;
constexpr std::uint32_t cube = 1u << 1u;
constexpr std::uint32_t volume = 1u << 2u;
constexpr std::uint32_t rt_texture = 1u << 3u;
constexpr std::uint32_t rt_surface = 1u << 4u;
constexpr std::uint32_t depth = 1u << 5u;
constexpr std::uint32_t vertex_texture = 1u << 6u;
constexpr std::uint32_t srgb_read = 1u << 7u;
constexpr std::uint32_t srgb_write = 1u << 8u;
constexpr std::uint32_t filter = 1u << 9u;
constexpr std::uint32_t blend = 1u << 10u;
constexpr std::uint32_t autogen = 1u << 11u;
constexpr std::uint32_t back_buffer = 1u << 12u;
constexpr std::uint32_t offscreen = 1u << 13u;
constexpr std::uint32_t bump = 1u << 14u;
constexpr std::uint32_t depth_texture = 1u << 15u;

constexpr std::uint32_t op_texture = 0x1u;
constexpr std::uint32_t op_offscreen_rt = 0x8u;
constexpr std::uint32_t op_same_format_rt = 0x10u;
constexpr std::uint32_t op_zstencil = 0x40u;
constexpr std::uint32_t op_zstencil_any_depth = 0x80u;
constexpr std::uint32_t op_up_to_alpha_rt = 0x100u;
constexpr std::uint32_t op_display = 0x400u;
constexpr std::uint32_t op_3d = 0x800u;
constexpr std::uint32_t op_offscreen_plain = 0x4000u;
constexpr std::uint32_t op_srgb_read = 0x8000u;
constexpr std::uint32_t op_bump = 0x10000u;
constexpr std::uint32_t op_no_filter = 0x40000u;
constexpr std::uint32_t op_member_argb = 0x80000u;
constexpr std::uint32_t op_srgb_write = 0x100000u;
constexpr std::uint32_t op_no_blend = 0x200000u;
constexpr std::uint32_t op_autogen = 0x400000u;
constexpr std::uint32_t op_vertex_texture = 0x800000u;

constexpr std::uint32_t a8r8g8b8 = 21u;
constexpr std::uint32_t x8r8g8b8 = 22u;
constexpr std::uint32_t r5g6b5 = 23u;
constexpr std::uint32_t x1r5g5b5 = 24u;
constexpr std::uint32_t a4r4g4b4 = 26u;
constexpr std::uint32_t d24s8 = 75u;
constexpr std::uint32_t r32f = 114u;
constexpr std::uint32_t v8u8 = 60u;

const Op *find(const D3d9FormatOps &ops, std::uint32_t format)
{
    for (std::uint32_t index = 0u; index < ops.count(); index++)
        if (ops.at(index).format_ == format) return &ops.at(index);
    return nullptr;
}

}

TEST(d3d9_format_ops, offers_nothing_the_host_has_not_answered)
{
    D3d9FormatOps ops;
    CHECK_EQ(ops.count(), 0u);
    CHECK(ops.formats_to_ask() > 10u);
    CHECK(ops.invariants_hold());
}

TEST(d3d9_format_ops, the_back_buffer_formats_are_display_modes)
{
    D3d9FormatOps ops;
    const std::uint32_t colour = texture | cube | volume | rt_texture | rt_surface | filter | blend | back_buffer |
                                 offscreen | srgb_read | srgb_write | vertex_texture | autogen;
    for (std::uint32_t index = 0u; index < ops.formats_to_ask(); index++) {
        const std::uint32_t format = ops.format_to_ask(index);
        ops.record(format, format == x8r8g8b8 || format == a8r8g8b8 || format == r5g6b5 ? colour : 0u, 0x14u);
    }
    CHECK(ops.invariants_hold());
    const Op *const x8 = find(ops, x8r8g8b8);
    CHECK(x8 != nullptr);
    CHECK((x8->operations_ & (op_display | op_3d)) == (op_display | op_3d));
    CHECK((x8->operations_ & op_texture) != 0u);
    CHECK((x8->operations_ & (op_offscreen_rt | op_same_format_rt)) == (op_offscreen_rt | op_same_format_rt));
    CHECK((x8->operations_ & op_member_argb) != 0u);
    CHECK((x8->operations_ & (op_srgb_read | op_srgb_write | op_autogen | op_vertex_texture)) ==
          (op_srgb_read | op_srgb_write | op_autogen | op_vertex_texture));
    CHECK((x8->operations_ & (op_no_filter | op_no_blend)) == 0u);
    CHECK_EQ(x8->multisample_, 0xbu);
    const Op *const a8 = find(ops, a8r8g8b8);
    CHECK(a8 != nullptr);
    CHECK((a8->operations_ & op_display) == 0u);
    CHECK((a8->operations_ & op_up_to_alpha_rt) != 0u);
    CHECK(find(ops, d24s8) == nullptr);
}

TEST(d3d9_format_ops, depth_bump_and_unfiltered_formats_say_so)
{
    D3d9FormatOps ops;
    for (std::uint32_t index = 0u; index < ops.formats_to_ask(); index++) {
        const std::uint32_t format = ops.format_to_ask(index);
        std::uint32_t       support = 0u;
        if (format == d24s8) support = depth | depth_texture | texture;
        if (format == r32f) support = texture | rt_texture | rt_surface;
        if (format == v8u8) support = texture | bump | filter;
        ops.record(format, support, 0u);
    }
    const Op *const zs = find(ops, d24s8);
    CHECK(zs != nullptr);
    CHECK((zs->operations_ & (op_zstencil | op_zstencil_any_depth)) == (op_zstencil | op_zstencil_any_depth));
    CHECK((zs->operations_ & op_texture) != 0u);
    CHECK((zs->operations_ & op_offscreen_rt) == 0u);
    const Op *const float_rt = find(ops, r32f);
    CHECK(float_rt != nullptr);
    CHECK((float_rt->operations_ & (op_no_filter | op_no_blend)) == (op_no_filter | op_no_blend));
    CHECK((float_rt->operations_ & op_offscreen_plain) == 0u);
    const Op *const bump_map = find(ops, v8u8);
    CHECK(bump_map != nullptr);
    CHECK((bump_map->operations_ & op_bump) != 0u);
    CHECK((bump_map->operations_ & op_no_filter) == 0u);
    CHECK(ops.invariants_hold());
}

TEST(d3d9_format_ops, a_second_answer_replaces_the_first)
{
    D3d9FormatOps ops;
    ops.record(x8r8g8b8, texture, 0u);
    ops.record(x8r8g8b8, texture | back_buffer, 0u);
    CHECK_EQ(ops.count(), 1u);
    CHECK((find(ops, x8r8g8b8)->operations_ & op_display) != 0u);
    ops.record(x8r8g8b8, 0u, 0u);
    CHECK_EQ(ops.count(), 0u);
    ops.record(7777u, texture, 0u);
    CHECK_EQ(ops.count(), 0u);
    CHECK(ops.invariants_hold());
}

TEST(d3d9_format_ops, nothing_offered_breaks_a_rule_the_runtime_disables_the_driver_over)
{
    D3d9FormatOps ops;
    const std::uint32_t everything = 0xffffu;
    for (std::uint32_t index = 0u; index < ops.formats_to_ask(); index++)
        ops.record(ops.format_to_ask(index), everything, 0x1fffeu);
    for (std::uint32_t index = 0u; index < ops.count(); index++) {
        const Op &op = ops.at(index);
        if ((op.operations_ & op_3d) != 0u) CHECK((op.operations_ & op_display) != 0u);
        if ((op.operations_ & op_display) != 0u) CHECK(op.format_ == x8r8g8b8 || op.format_ == r5g6b5);
        CHECK(!(op.format_ == x1r5g5b5 && (op.operations_ & op_display) != 0u));
        if ((op.operations_ & op_member_argb) != 0u) CHECK(op.format_ != a4r4g4b4);
        if (op.multisample_ != 0u) CHECK((op.multisample_ & 1u) != 0u);
    }
    CHECK(find(ops, x8r8g8b8) != nullptr);
    CHECK((find(ops, a8r8g8b8)->operations_ & op_3d) == 0u);
}

TEST(d3d9_format_ops, a_question_past_the_asked_list_names_no_format)
{
    D3d9FormatOps ops;
    CHECK(ops.format_to_ask(ops.formats_to_ask() - 1u) != 0u);
    CHECK_EQ(ops.format_to_ask(ops.formats_to_ask()), 0u);
}

TEST(d3d9_format_ops, a_corrupted_offer_list_breaks_the_invariants)
{
    D3d9FormatOps ops;
    D3d9FormatOpsProbe::set_count(ops, D3d9FormatOps::max_formats + 1u);
    CHECK(!ops.invariants_hold());
    D3d9FormatOpsProbe::set_count(ops, 1u);
    D3d9FormatOpsProbe::set(ops, 0u, x8r8g8b8, 0u);
    CHECK(!ops.invariants_hold());
    D3d9FormatOpsProbe::set(ops, 0u, 7777u, op_texture);
    CHECK(!ops.invariants_hold());
    D3d9FormatOpsProbe::set(ops, 0u, x8r8g8b8, op_texture);
    CHECK(ops.invariants_hold());
    D3d9FormatOpsProbe::set_count(ops, 2u);
    D3d9FormatOpsProbe::set(ops, 1u, x8r8g8b8, op_texture);
    CHECK(!ops.invariants_hold());
    D3d9FormatOpsProbe::set(ops, 1u, a8r8g8b8, op_texture);
    CHECK(ops.invariants_hold());
}
