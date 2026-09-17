/* WGL_ARB_pixel_format, with WGL_ARB_multisample's and
 * WGL_ARB_framebuffer_sRGB's attributes, answered from the ICD's pixel formats
 * (data-model.md): the attributes of one format, and the formats a criteria
 * list selects in the ARB's order - exact criteria must be equal, minimum ones
 * met, and among the formats that pass, those with the fewest bits past the
 * minimums come first, in table order otherwise. Criteria the ARB ignores
 * (the format count, the channel shifts, transparent values) select nothing.
 *
 * Every list is read as bytes the application wrote: pairs until a zero name,
 * at most max_pairs of them. */
#ifndef AV_GLICD_PIXEL_FORMAT_QUERY_HPP
#define AV_GLICD_PIXEL_FORMAT_QUERY_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av::glicd {

class PixelFormat;
class PixelFormatCriteria;
class PixelFormatTable;

class PixelFormatQuery {
public:
    static constexpr uint32_t max_pairs = 128u;
    enum : uint32_t {
        number_pixel_formats = 0x2000u, draw_to_window = 0x2001u, draw_to_bitmap = 0x2002u,
        acceleration = 0x2003u, need_palette = 0x2004u, need_system_palette = 0x2005u,
        swap_layer_buffers = 0x2006u, swap_method = 0x2007u, number_overlays = 0x2008u,
        number_underlays = 0x2009u, transparent = 0x200Au, share_depth = 0x200Cu, share_stencil = 0x200Du,
        share_accum = 0x200Eu, support_gdi = 0x200Fu, support_opengl = 0x2010u, double_buffer = 0x2011u,
        stereo = 0x2012u, pixel_type = 0x2013u, color_bits = 0x2014u, red_bits = 0x2015u, red_shift = 0x2016u,
        green_bits = 0x2017u, green_shift = 0x2018u, blue_bits = 0x2019u, blue_shift = 0x201Au,
        alpha_bits = 0x201Bu, alpha_shift = 0x201Cu, accum_bits = 0x201Du, accum_alpha_bits = 0x2021u,
        depth_bits = 0x2022u, stencil_bits = 0x2023u, aux_buffers = 0x2024u, full_acceleration = 0x2027u,
        swap_undefined = 0x202Au, type_rgba = 0x202Bu, draw_to_pbuffer = 0x202Du,
        transparent_red_value = 0x2037u, transparent_index_value = 0x203Bu, sample_buffers = 0x2041u,
        samples = 0x2042u, framebuffer_srgb_capable = 0x20A9u,
    };

    explicit PixelFormatQuery(const PixelFormatTable &table);
    PixelFormatQuery(const PixelFormatQuery &) = delete;
    PixelFormatQuery &operator=(const PixelFormatQuery &) = delete;

    /* wglGetPixelFormatAttribiv/fvARB: the `count` attributes `names` names
     * of format `format` (1-based) into `ints` or `floats`, exactly one of
     * them non-null. Format 0 answers only WGL_NUMBER_PIXEL_FORMATS_ARB.
     * not_found for a format out of range; invalid_parameter for a layer
     * other than 0, a missing list or output, more than max_pairs names or an
     * unknown one. Nothing is written unless ok. */
    [[nodiscard]] Status attribs(int32_t format, int32_t layer, uint32_t count, const int32_t *names, int32_t *ints,
                                 float *floats) const;
    /* wglChoosePixelFormatARB: at most `max` chosen formats into `formats`
     * and how many formats the criteria select into `count`. A null list asks
     * nothing. invalid_parameter, writing nothing, for an unknown name, a
     * float name that is not a whole number, a list with no zero name within
     * max_pairs pairs, a null `count`, or a null `formats` with `max` above 0. */
    [[nodiscard]] Status choose(const int32_t *ints, const float *floats, uint32_t max, int32_t *formats,
                                uint32_t *count) const;

    bool invariants_hold() const;

private:
    enum class Match : uint8_t { unknown, ignored, exact, minimum };

    Match   match_of(uint32_t name) const;
    int32_t value_of(const PixelFormat &row, uint32_t name) const;
    [[nodiscard]] Status read_ints(const int32_t *list, PixelFormatCriteria &criteria) const;
    [[nodiscard]] Status read_floats(const float *list, PixelFormatCriteria &criteria) const;
    /* Whether `row` meets every criterion, and its bits past the minimums. */
    bool    admits(const PixelFormat &row, const PixelFormatCriteria &criteria, double &surplus) const;

    const PixelFormatTable &table_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_PIXEL_FORMAT_QUERY_HPP
