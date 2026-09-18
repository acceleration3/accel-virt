#include "guest/suite/gl_probe.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/bmp_image.hpp"
#include "guest/suite/gl_entry.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/image_sink.hpp"

namespace av {

namespace {

/* triangle.vert and solid.frag for OpenGL, which numbers vertices with
 * gl_VertexID rather than gl_VertexIndex. */
constexpr const char *triangle_source = "#version 330 core\n"
                                        "void main() {\n"
                                        "    vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
                                        "    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);\n"
                                        "}\n";
constexpr const char *solid_source = "#version 330 core\n"
                                     "out vec4 colour;\n"
                                     "void main() {\n"
                                     "    colour = vec4(0.25, 0.5, 0.75, 1.0);\n"
                                     "}\n";

constexpr uint32_t lowest_major = 3u, highest_major = 4u, highest_minor = 6u, bytes_per_pixel = 4u;
constexpr uint32_t version_scale = 10u;

/* WGL_ARB_create_context(_profile, _robustness) names. */
constexpr int32_t major_version = 0x2091, minor_version = 0x2092, profile_mask = 0x9126;
constexpr int32_t context_flags = 0x2094, robust_access_bit = 0x4;
constexpr int32_t reset_strategy = 0x8256, lose_context_on_reset = 0x8252;
constexpr int32_t both_profiles = 3;

constexpr uint8_t share_texels[GlProbe::share_extent * GlProbe::share_extent * bytes_per_pixel] = {
    1u, 2u, 3u, 255u, 40u, 50u, 60u, 255u, 70u, 80u, 90u, 255u, 200u, 210u, 220u, 255u};

/* Leading digits as a number, and where they end; `end` null when there are none. */
uint32_t number_at(const char *text, const char *&end)
{
    AV_SCOPE("number_at");
    uint32_t value = 0u;
    end = nullptr;
    for (; *text >= '0' && *text <= '9' && value < version_scale * version_scale; text++) {
        value = value * version_scale + static_cast<uint32_t>(*text - '0');
        end = text + 1;
    }
    return value;
}

/* How many of `count` RGBA pixels are GlProbe::colour within 1. */
uint32_t matching(const uint8_t *rgba, uint32_t count)
{
    AV_SCOPE("matching");
    uint32_t matched = 0u;
    for (uint32_t pixel = 0u; pixel < count; pixel++) {
        bool close = true;
        for (uint32_t channel = 0u; channel < bytes_per_pixel; channel++) {
            const int32_t difference = static_cast<int32_t>(rgba[pixel * bytes_per_pixel + channel]) -
                                       static_cast<int32_t>(GlProbe::colour[channel]);
            close = close && difference >= -1 && difference <= 1;
        }
        matched += close ? 1u : 0u;
    }
    return matched;
}

}  // namespace

GlProbe::GlProbe(Kmt &kmt, GuestReport &report, GlEntry &gl, ImageSink &images)
    : kmt_(kmt), report_(report), gl_(gl), images_(images), steps_(kmt, report, "opengl")
{
    AV_SCOPE("GlProbe::GlProbe");
}

void GlProbe::run()
{
    AV_SCOPE("GlProbe::run");
    if (pixel_format_step() != Status::ok) return;
    void *context = nullptr;
    if (context_step(context) != Status::ok) return;
    version_step();
    draw_step("draw");
    share_step(context);
    busy_step(context);
    /* opengl32 names an extension function only to a thread with a context
     * current, so the first context stays current while the attribute
     * contexts are made; the forced loss takes it down with its device. */
    compat_step();
    interval_step();
    resized_step();
    attribs_step();
    lost_step();
    static_cast<void>(gl_.make_current(nullptr));
    static_cast<void>(gl_.delete_context(context));
    recreated_step();
}

Status GlProbe::pixel_format_step()
{
    AV_SCOPE("GlProbe::pixel_format_step");
    const int32_t format = gl_.choose_pixel_format();
    const bool    set = format > 0 && gl_.set_pixel_format(format);
    return steps_.check_measured("pixel_format", set ? Status::ok : Status::not_found,
                                 set ? static_cast<uint32_t>(format) : gl_.last_error());
}

Status GlProbe::context_step(void *&context)
{
    AV_SCOPE("GlProbe::context_step");
    context = gl_.create_context();
    if (context != nullptr && gl_.make_current(context)) return steps_.check("context", Status::ok);
    const uint32_t error = gl_.last_error();
    if (context != nullptr) static_cast<void>(gl_.delete_context(context));
    context = nullptr;
    return steps_.check_measured("context", Status::invalid_state, error);
}

void GlProbe::version_step()
{
    AV_SCOPE("GlProbe::version_step");
    const char *const version = gl_.string(gl_version);
    const char *const renderer = gl_.string(gl_renderer);
    report_.note("opengl", "version", version);
    report_.note("opengl", "renderer", renderer);
    const char    *end = nullptr;
    const uint32_t major = version != nullptr ? number_at(version, end) : 0u;
    const bool     dotted = end != nullptr && *end == '.';
    const uint32_t minor = dotted ? number_at(end + 1, end) : 0u;
    const bool     parsed = dotted && end != nullptr;
    const bool     supported = parsed && major >= lowest_major &&
                           (major < highest_major || (major == highest_major && minor <= highest_minor));
    const bool named = renderer != nullptr && renderer[0] != '\0';
    steps_.check_measured("version", supported && named ? Status::ok : Status::not_supported,
                          major * version_scale + minor);
}

void GlProbe::draw_step(const char *name)
{
    AV_SCOPE("GlProbe::draw_step");
    const uint32_t width = gl_.width() < max_extent ? gl_.width() : max_extent;
    const uint32_t height = gl_.height() < max_extent ? gl_.height() : max_extent;
    const uint32_t made = program();
    if (made == 0u) {
        static_cast<void>(steps_.check_measured(name, Status::invalid_state, 0u));
        return;
    }
    const uint32_t array = gl_.gen_vertex_array();
    gl_.bind_vertex_array(array);
    gl_.use_program(made);
    gl_.viewport(0, 0, static_cast<int32_t>(gl_.width()), static_cast<int32_t>(gl_.height()));
    gl_.clear_colour(0.0f, 0.0f, 0.0f, 1.0f);
    gl_.clear(gl_color_buffer_bit);
    gl_.draw_arrays(gl_triangles, 0, 3);
    const bool swapped = gl_.swap_buffers();
    gl_.finish();
    uint8_t pixels[max_extent * max_extent * bytes_per_pixel] = {};
    gl_.read_pixels(static_cast<int32_t>(width), static_cast<int32_t>(height), pixels);
    const uint32_t error = gl_.error();
    gl_.use_program(0u);
    gl_.bind_vertex_array(0u);
    gl_.delete_vertex_array(array);
    gl_.delete_program(made);
    char       picture[ImageSink::max_name];
    TextBuffer named(picture, sizeof picture);
    named.append("opengl-");
    named.append(name);
    /* glReadPixels returns the bottom row first. */
    images_.save(named.text(), BmpImage(pixels, width, height, BmpImage::Rows::bottom_first));
    const uint32_t count = width * height;
    const uint32_t matched = matching(pixels, count);
    const bool     drawn = swapped && error == 0u && count != 0u && matched == count;
    static_cast<void>(steps_.check_measured(name, drawn ? Status::ok : Status::invalid_state, matched));
}

/* Feature 051, US1: the profile the host granted, and the fixed-function
 * pipeline drawn through a display list on it. The measurement is the mask, so
 * a report says which profile the run had; a core context is not a failure of
 * this driver, it is a host with no compatibility profile, and the step says
 * so with the pixels it could not draw. */
void GlProbe::compat_step()
{
    AV_SCOPE("GlProbe::compat_step");
    const int32_t mask = gl_.integer(gl_context_profile_mask);
    const bool    compatibility = (static_cast<uint32_t>(mask) & gl_compatibility_profile_bit) != 0u;
    if (!compatibility) {
        report_.note("opengl", "compat", "the host granted no compatibility profile");
        static_cast<void>(steps_.check_measured("compat", Status::not_supported, static_cast<uint32_t>(mask)));
        return;
    }
    const uint32_t width = gl_.width() < max_extent ? gl_.width() : max_extent;
    const uint32_t height = gl_.height() < max_extent ? gl_.height() : max_extent;
    gl_.viewport(0, 0, static_cast<int32_t>(gl_.width()), static_cast<int32_t>(gl_.height()));
    gl_.clear_colour(0.0f, 0.0f, 0.0f, 1.0f);
    gl_.clear(gl_color_buffer_bit);
    const bool drawn = gl_.legacy_triangle();
    const bool swapped = drawn && gl_.swap_buffers();
    uint8_t    pixels[max_extent * max_extent * bytes_per_pixel] = {};
    gl_.read_pixels(static_cast<int32_t>(width), static_cast<int32_t>(height), pixels);
    const uint32_t error = gl_.error();
    images_.save("opengl-compat", BmpImage(pixels, width, height, BmpImage::Rows::bottom_first));
    const uint32_t count = width * height;
    const uint32_t matched = matching(pixels, count);
    const bool     whole = swapped && error == 0u && count != 0u && matched == count;
    static_cast<void>(steps_.check_measured("compat", whole ? Status::ok : Status::invalid_state, matched));
}

/* Feature 051, US2.2: sixty swaps at interval 1 cannot have taken less time
 * than fifty-nine vertical blanks, and interval 0 waits for none. */
void GlProbe::interval_step()
{
    AV_SCOPE("GlProbe::interval_step");
    if (!gl_.set_swap_interval(paced_interval) || gl_.swap_interval() != paced_interval) {
        static_cast<void>(steps_.check_measured("interval", Status::not_supported, 0u));
        return;
    }
    const uint64_t period = blank_ticks();
    if (period == 0u) {
        static_cast<void>(steps_.check_measured("interval", Status::not_found, 0u));
        return;
    }
    const uint64_t started = counter();
    for (uint32_t frame = 0u; frame < paced_frames; frame++) static_cast<void>(gl_.swap_buffers());
    const uint64_t paced = counter() - started;
    const uint64_t least = period * (paced_frames - 1u);
    /* And with no interval the same swaps are not held to the blank. */
    const bool     unpaced_set = gl_.set_swap_interval(0);
    const uint64_t loose_started = counter();
    for (uint32_t frame = 0u; frame < paced_frames; frame++) static_cast<void>(gl_.swap_buffers());
    const uint64_t unpaced = counter() - loose_started;
    static_cast<void>(gl_.set_swap_interval(paced_interval));
    const bool held = paced >= least && unpaced_set && unpaced < paced;
    static_cast<void>(steps_.check_measured("interval", held ? Status::ok : Status::conflict,
                                            static_cast<uint32_t>(paced / period)));
}

/* Feature 051, US2.3: a window resized under a context that keeps swapping is
 * presented at its new extent, and nothing had to be made again by hand. */
void GlProbe::resized_step()
{
    AV_SCOPE("GlProbe::resized_step");
    const uint32_t width = gl_.width();
    const uint32_t height = gl_.height();
    const uint32_t narrowed = width / 2u;
    if (narrowed == 0u || narrowed == width || !gl_.resize(narrowed, height)) {
        static_cast<void>(steps_.check_measured("resized", Status::not_supported, 0u));
        return;
    }
    /* The first swap presents what the window had; the second is the first at
     * the new extent. */
    static_cast<void>(gl_.swap_buffers());
    gl_.viewport(0, 0, static_cast<int32_t>(gl_.width()), static_cast<int32_t>(gl_.height()));
    gl_.clear_colour(0.0f, 0.0f, 0.0f, 1.0f);
    gl_.clear(gl_color_buffer_bit);
    const bool     swapped = gl_.swap_buffers();
    const uint32_t error = gl_.error();
    const bool     resized = gl_.width() == narrowed && swapped && error == 0u;
    static_cast<void>(steps_.check_measured("resized", resized ? Status::ok : Status::invalid_state, gl_.width()));
    static_cast<void>(gl_.resize(width, height));
}

uint64_t GlProbe::blank_ticks()
{
    AV_SCOPE("GlProbe::blank_ticks");
    const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                             AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
    AdapterSearch     search(kmt_);
    if (search.find(rvkgpu) != Status::ok) return 0u;
    uint32_t device = 0u;
    uint64_t ticks = 0u;
    if (kmt_.create_device(search.adapter(), device) == Status::ok) {
        if (kmt_.wait_for_vertical_blank(search.adapter(), device, 0u) == Status::ok) {
            const uint64_t started = counter();
            if (kmt_.wait_for_vertical_blank(search.adapter(), device, 0u) == Status::ok) ticks = counter() - started;
        }
        static_cast<void>(kmt_.destroy_device(device));
    }
    static_cast<void>(search.close());
    return ticks;
}

uint64_t GlProbe::counter() const
{
    AV_SCOPE("GlProbe::counter");
    uint64_t frequency = 0u;
    return kmt_.performance_counter(frequency);
}

/* Both contexts are shared before either is made, as the ICD requires. */
void GlProbe::share_step(void *current)
{
    AV_SCOPE("GlProbe::share_step");
    void *const first = gl_.create_context();
    void *const second = gl_.create_context();
    uint8_t     read[sizeof share_texels] = {};
    uint32_t    matched = 0u;
    bool        shared = first != nullptr && second != nullptr && gl_.share_lists(first, second) &&
                  gl_.make_current(first);
    uint32_t    failure = shared ? 0u : gl_.last_error();
    if (shared) {
        const uint32_t texture = gl_.gen_texture();
        gl_.bind_texture(texture);
        gl_.texture_image(share_extent, share_extent, share_texels);
        gl_.finish();
        shared = gl_.make_current(second);
        failure = shared ? 0u : gl_.last_error();
        if (shared) {
            gl_.bind_texture(texture);
            gl_.read_texture(read);
            images_.save("opengl-share", BmpImage(read, share_extent, share_extent, BmpImage::Rows::bottom_first));
            for (uint32_t at = 0u; at < sizeof read; at += bytes_per_pixel)
                matched += __builtin_memcmp(read + at, share_texels + at, bytes_per_pixel) == 0 ? 1u : 0u;
        }
        /* Deleted from whichever context shares it. */
        gl_.bind_texture(0u);
        gl_.delete_texture(texture);
    }
    static_cast<void>(gl_.make_current(current));
    if (second != nullptr) static_cast<void>(gl_.delete_context(second));
    if (first != nullptr) static_cast<void>(gl_.delete_context(first));
    const uint32_t texels = share_extent * share_extent;
    if (!shared) {
        static_cast<void>(steps_.check_measured("share", Status::invalid_state, failure));
        return;
    }
    static_cast<void>(steps_.check_measured("share", matched == texels ? Status::ok : Status::invalid_state,
                                            matched));
}

void GlProbe::busy_step(void *current)
{
    AV_SCOPE("GlProbe::busy_step");
    const uint32_t error = gl_.make_current_elsewhere(current);
    static_cast<void>(steps_.check_measured("busy", error == error_busy ? Status::ok : Status::invalid_state, error));
}

/* The fault loses every context on the device, so nothing else is current. */
void GlProbe::lost_step()
{
    AV_SCOPE("GlProbe::lost_step");
    const int32_t robust[] = {context_flags, robust_access_bit, reset_strategy, lose_context_on_reset, 0};
    void *const   context = gl_.create_context_attribs(robust);
    if (context == nullptr || !gl_.make_current(context)) {
        const uint32_t error = gl_.last_error();
        if (context != nullptr) static_cast<void>(gl_.delete_context(context));
        static_cast<void>(steps_.check_measured("lost", Status::invalid_state, error));
        return;
    }
    const int32_t  forced = gl_.force_fault();
    const uint32_t status = forced == 1 ? gl_.reset_status() : 0u;
    const uint32_t reported = gl_.error();
    const uint32_t after = gl_.error();
    static_cast<void>(gl_.make_current(nullptr));
    static_cast<void>(gl_.delete_context(context));
    const bool lost = forced == 1 && status == gl_unknown_context_reset && reported == gl_context_lost && after == 0u;
    static_cast<void>(steps_.check_measured("lost", lost ? Status::ok : Status::invalid_state, status));
}

void GlProbe::recreated_step()
{
    AV_SCOPE("GlProbe::recreated_step");
    void *const context = gl_.create_context();
    if (context == nullptr || !gl_.make_current(context)) {
        const uint32_t error = gl_.last_error();
        if (context != nullptr) static_cast<void>(gl_.delete_context(context));
        static_cast<void>(steps_.check_measured("recreated", Status::invalid_state, error));
        return;
    }
    draw_step("recreated");
    static_cast<void>(gl_.make_current(nullptr));
    static_cast<void>(gl_.delete_context(context));
}

void GlProbe::attribs_step()
{
    AV_SCOPE("GlProbe::attribs_step");
    const int32_t too_high[] = {major_version, 4, minor_version, 7, 0};
    const int32_t two_profiles[] = {major_version, 3, minor_version, 3, profile_mask, both_profiles, 0};
    void *const   higher = gl_.create_context_attribs(too_high);
    const uint32_t version_error = higher == nullptr ? gl_.last_error() : 0u;
    if (higher != nullptr) static_cast<void>(gl_.delete_context(higher));
    void *const    profiled = gl_.create_context_attribs(two_profiles);
    const uint32_t profile_error = profiled == nullptr ? gl_.last_error() : 0u;
    if (profiled != nullptr) static_cast<void>(gl_.delete_context(profiled));
    if (version_error != error_invalid_version_arb) {
        static_cast<void>(steps_.check_measured("attribs", Status::invalid_state, version_error));
        return;
    }
    const Status refused = profile_error == error_invalid_profile_arb ? Status::ok : Status::invalid_state;
    static_cast<void>(steps_.check_measured("attribs", refused, profile_error));
}

uint32_t GlProbe::program()
{
    AV_SCOPE("GlProbe::program");
    const uint32_t vertex = shader(gl_vertex_shader, triangle_source);
    const uint32_t fragment = shader(gl_fragment_shader, solid_source);
    uint32_t       made = vertex != 0u && fragment != 0u ? gl_.create_program() : 0u;
    if (made != 0u) {
        gl_.attach_shader(made, vertex);
        gl_.attach_shader(made, fragment);
        gl_.link_program(made);
        if (gl_.program_parameter(made, gl_link_status) == 0) {
            gl_.delete_program(made);
            made = 0u;
        }
    }
    if (vertex != 0u) gl_.delete_shader(vertex);
    if (fragment != 0u) gl_.delete_shader(fragment);
    return made;
}

uint32_t GlProbe::shader(uint32_t type, const char *source)
{
    AV_SCOPE("GlProbe::shader");
    const uint32_t made = gl_.create_shader(type);
    if (made == 0u) return 0u;
    gl_.shader_source(made, source);
    gl_.compile_shader(made);
    if (gl_.shader_parameter(made, gl_compile_status) != 0) return made;
    gl_.delete_shader(made);
    return 0u;
}


}  // namespace av
