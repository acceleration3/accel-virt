#include "icd/opengl/pixel_format_query.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/pixel_format.hpp"
#include "icd/opengl/pixel_format_criteria.hpp"
#include "icd/opengl/pixel_format_table.hpp"

namespace av::glicd {

namespace {

constexpr int32_t color_bit_count = 32;
constexpr int32_t channel_bit_count = 8;
constexpr int32_t red_offset = 16;
constexpr int32_t green_offset = 8;
constexpr int32_t alpha_offset = 24;
constexpr float   name_limit = 4294967296.0f;

}

PixelFormatQuery::PixelFormatQuery(const PixelFormatTable &table) : table_(table)
{
    AV_SCOPE("PixelFormatQuery::PixelFormatQuery");
}

Status PixelFormatQuery::attribs(int32_t format, int32_t layer, uint32_t count, const int32_t *names, int32_t *ints,
                                 float *floats) const
{
    AV_SCOPE("PixelFormatQuery::attribs");
    if (layer != 0 || count > max_pairs || (ints == nullptr) == (floats == nullptr)) return Status::invalid_parameter;
    if (count != 0u && names == nullptr) return Status::invalid_parameter;
    for (uint32_t at = 0u; at < count; at++) {
        const uint32_t name = static_cast<uint32_t>(names[at]);
        if (match_of(name) == Match::unknown) return Status::invalid_parameter;
        const bool counted = format == 0 && name == number_pixel_formats;
        if (!counted && !table_.contains(format)) return Status::not_found;
    }
    for (uint32_t at = 0u; at < count; at++) {
        const uint32_t name = static_cast<uint32_t>(names[at]);
        const int32_t  value = name == number_pixel_formats ? static_cast<int32_t>(table_.count())
                                                            : value_of(table_.format(format), name);
        if (ints != nullptr) ints[at] = value;
        if (floats != nullptr) floats[at] = static_cast<float>(value);
    }
    return Status::ok;
}

Status PixelFormatQuery::choose(const int32_t *ints, const float *floats, uint32_t max, int32_t *formats,
                                uint32_t *count) const
{
    AV_SCOPE("PixelFormatQuery::choose");
    if (count == nullptr || (formats == nullptr && max != 0u)) return Status::invalid_parameter;
    PixelFormatCriteria criteria;
    if (read_ints(ints, criteria) != Status::ok || read_floats(floats, criteria) != Status::ok)
        return Status::invalid_parameter;
    uint32_t chosen[PixelFormatCriteria::capacity];
    double   surpluses[PixelFormatCriteria::capacity];
    uint32_t matched = 0u;
    for (int32_t format = 1; table_.contains(format); format++) {
        double surplus = 0.0;
        if (!admits(table_.format(format), criteria, surplus)) continue;
        uint32_t at = matched;
        for (; at != 0u && surpluses[at - 1u] > surplus; at--) {
            chosen[at] = chosen[at - 1u];
            surpluses[at] = surpluses[at - 1u];
        }
        chosen[at] = static_cast<uint32_t>(format);
        surpluses[at] = surplus;
        matched++;
    }
    for (uint32_t at = 0u; at < matched && at < max; at++) formats[at] = static_cast<int32_t>(chosen[at]);
    *count = matched;
    return Status::ok;
}

bool PixelFormatQuery::invariants_hold() const
{
    AV_SCOPE("PixelFormatQuery::invariants_hold");
    return table_.invariants_hold();
}

PixelFormatQuery::Match PixelFormatQuery::match_of(uint32_t name) const
{
    AV_SCOPE("PixelFormatQuery::match_of");
    switch (name) {
    case number_pixel_formats: case red_shift: case green_shift: case blue_shift: case alpha_shift:
        return Match::ignored;
    case draw_to_window: case draw_to_bitmap: case acceleration: case need_palette: case need_system_palette:
    case swap_layer_buffers: case swap_method: case transparent: case share_depth: case share_stencil:
    case share_accum: case support_gdi: case support_opengl: case double_buffer: case stereo: case pixel_type:
    case draw_to_pbuffer: case framebuffer_srgb_capable:
        return Match::exact;
    case number_overlays: case number_underlays: case color_bits: case red_bits: case green_bits: case blue_bits:
    case alpha_bits: case depth_bits: case stencil_bits: case aux_buffers: case sample_buffers: case samples:
        return Match::minimum;
    default:
        break;
    }
    if (name >= accum_bits && name <= accum_alpha_bits) return Match::minimum;
    if (name >= transparent_red_value && name <= transparent_index_value) return Match::ignored;
    return Match::unknown;
}

int32_t PixelFormatQuery::value_of(const PixelFormat &row, uint32_t name) const
{
    AV_SCOPE("PixelFormatQuery::value_of");
    switch (name) {
    case draw_to_window: case support_opengl: return 1;
    case acceleration: return static_cast<int32_t>(full_acceleration);
    case swap_method: return static_cast<int32_t>(swap_undefined);
    case double_buffer: return row.double_buffer() ? 1 : 0;
    case pixel_type: return static_cast<int32_t>(type_rgba);
    case color_bits: return color_bit_count;
    case red_bits: case green_bits: case blue_bits: case alpha_bits: return channel_bit_count;
    case red_shift: return red_offset;
    case green_shift: return green_offset;
    case alpha_shift: return alpha_offset;
    case depth_bits: return row.depth_bits();
    case stencil_bits: return row.stencil_bits();
    case sample_buffers: return row.samples() != 0u ? 1 : 0;
    case samples: return row.samples();
    case framebuffer_srgb_capable: return row.srgb() ? 1 : 0;
    default: return 0;
    }
}

Status PixelFormatQuery::read_ints(const int32_t *list, PixelFormatCriteria &criteria) const
{
    AV_SCOPE("PixelFormatQuery::read_ints");
    if (list == nullptr) return Status::ok;
    for (uint32_t pair = 0u;; pair++) {
        const uint32_t name = static_cast<uint32_t>(list[2u * pair]);
        if (name == 0u) return Status::ok;
        if (pair == max_pairs || match_of(name) == Match::unknown) return Status::invalid_parameter;
        static_cast<void>(criteria.add(name, static_cast<double>(list[2u * pair + 1u])));
    }
}

Status PixelFormatQuery::read_floats(const float *list, PixelFormatCriteria &criteria) const
{
    AV_SCOPE("PixelFormatQuery::read_floats");
    if (list == nullptr) return Status::ok;
    for (uint32_t pair = 0u;; pair++) {
        const float raw = list[2u * pair];
        if (raw == 0.0f) return Status::ok;
        if (pair == max_pairs || !(raw >= 1.0f && raw < name_limit)) return Status::invalid_parameter;
        const uint32_t name = static_cast<uint32_t>(raw);
        if (static_cast<float>(name) != raw || match_of(name) == Match::unknown) return Status::invalid_parameter;
        static_cast<void>(criteria.add(name, static_cast<double>(list[2u * pair + 1u])));
    }
}

bool PixelFormatQuery::admits(const PixelFormat &row, const PixelFormatCriteria &criteria, double &surplus) const
{
    AV_SCOPE("PixelFormatQuery::admits");
    for (uint32_t at = 0u; at < criteria.count(); at++) {
        const uint32_t name = criteria.name(at);
        const double   have = static_cast<double>(value_of(row, name));
        const double   want = criteria.value(at);
        const Match    match = match_of(name);
        if (match == Match::exact && have != want) return false;
        if (match != Match::minimum) continue;
        if (have < want) return false;
        surplus += have - want;
    }
    return true;
}

}
