/* Target-independent rasterization (D3D11.1: ForcedSampleCount with a
 * single-sample render target, or with no target at all) on every GPU here
 * that runs Direct3D, through DXVK: Direct2D draws its anti-aliased geometry
 * this way, a pixel shader writing SV_Coverage into an R*_UINT target with
 * the logic op OR'ing or XOR'ing the masks, and reads the bits back as
 * coverage. No Vulkan driver rasterizes 16 samples into one, so DXVK
 * emulates it (D3D11TirShaders): what these tests hold it to is a D3D
 * rasterizer's answer, sample for sample.
 *
 * The reference here is that rasterizer for one pixel: vertices snapped to
 * 16.8 fixed point, edge functions with the top-left rule, D3D's standard
 * sample patterns, clip distances interpolated to every sample. The geometry
 * puts edges exactly through sample positions, so a rule broken anywhere -
 * a top edge counted, a shared edge counted twice, a sample off by a
 * subpixel - changes a bit.
 *
 * DXVK is loaded from AV_D3D_PATH, or from build-d3d/lib in this checkout
 * (tools/build_d3d_runtime.sh). A host without it returns without judging. */
#include "fixtures/d3d/tir_shaders.hpp"
#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libvulkan\n"
           "leak:libdxvk\n"
           "leak:libvulkan_radeon\n"
           "leak:libvkd3d\n"
           "leak:libz\n";
}

namespace {

using namespace av::d3dproto;

constexpr std::uint32_t format_r32_uint = 42u;
constexpr std::uint32_t format_r32g32b32a32 = 2u;
constexpr std::uint32_t format_r32g32 = 16u;
constexpr std::uint32_t bind_vertex = 0x1u;
constexpr std::uint32_t bind_render_target = 0x20u;
constexpr std::uint32_t bind_unordered_access = 0x80u;
constexpr std::uint32_t view_texture2d = 4u;
constexpr std::uint32_t topology_triangles = 4u;
constexpr std::uint32_t fill_solid = 3u;
constexpr std::uint32_t cull_none = 1u;
constexpr std::uint32_t logic_op_or = 8u;
constexpr std::uint32_t logic_op_xor = 10u;
constexpr std::uint32_t blend_one = 2u;
constexpr std::uint32_t blend_zero = 1u;
constexpr std::uint32_t blend_op_add = 1u;
constexpr std::uint32_t size = 64u;
/* Subpixels per pixel: D3D's 16.8 fixed point. */
constexpr std::int64_t subpixels = 256;
/* What the target is cleared to where a test needs to see untouched pixels:
 * no pattern sets the high half. */
constexpr std::uint32_t untouched = 0xffff0000u;
constexpr std::uint32_t max_triangles = 256u;
/* The forced sample counts besides 1. */
constexpr std::uint32_t forced_counts[] = {2u, 4u, 8u, 16u};
constexpr std::uint32_t multisample_counts[] = {4u, 8u, 16u};
constexpr std::uint32_t high_counts[] = {8u, 16u};

/* Wire object ids. */
enum : std::uint32_t {
    target_id = 1u,
    target_view_id,
    uav_texture_id,
    uav_view_id,
    vertices_id,
    vs_id,
    coverage_ps_id,
    uav_ps_id,
    attribute_vs_id,
    eval_ps_id,
    clip_ps_id,
    layout_id,
    attribute_layout_id,
    blend_or_id,
    blend_xor_id,
    blend_write_id,
    rasterizer_normal_id,
    rasterizer_forced_base_id,
};

/* A vertex in pixels, its clip-space w, and two clip distances. */
struct Vertex {
    double x, y;
    double clip0, clip1;
    double w;
};

struct Triangle {
    Vertex v[3];
};

/* What the vertex buffer holds: the clip-space position and the clip
 * distances. */
struct WireVertex {
    float x, y, z, w;
    float clip0, clip1;
};

struct Sample {
    int x, y;
};

/* D3D's standard sample patterns, in 1/16 pixel from the pixel center. */
constexpr Sample pattern2[2] = {{4, 4}, {-4, -4}};
constexpr Sample pattern4[4] = {{-2, -6}, {6, -2}, {-6, 2}, {2, 6}};
constexpr Sample pattern8[8] = {{1, -3}, {-1, 3}, {5, 1}, {-3, -5}, {-5, 5}, {-7, -1}, {3, 7}, {7, -7}};
constexpr Sample pattern16[16] = {{1, 1},   {-1, -3}, {-3, 2}, {4, -1}, {-5, -2}, {2, 5},  {5, 3}, {3, -5},
                                  {-2, 6},  {0, -7},  {-4, -6}, {-6, 4}, {-8, 0}, {7, -4}, {6, 7}, {-7, -8}};

const Sample *pattern(std::uint32_t samples)
{
    switch (samples) {
    case 2u: return pattern2;
    case 4u: return pattern4;
    case 8u: return pattern8;
    default: return pattern16;
    }
}

/* D3D's rasterizer for one triangle and one forced pattern, the way the
 * functional spec defines it: x and y snapped to 16.8 fixed point (round to
 * nearest even), a sample inside when every edge function is positive or
 * zero on a top or left edge (y down), and kept when every clip distance
 * interpolated to it is at least zero. */
class ReferenceTriangle {
public:
    ReferenceTriangle(const Triangle &triangle, std::uint32_t samples) : samples_(samples), valid_(false)
    {
        for (std::uint32_t i = 0u; i < 3u; i++) {
            x_[i] = static_cast<std::int64_t>(std::nearbyint(triangle.v[i].x * double(subpixels)));
            y_[i] = static_cast<std::int64_t>(std::nearbyint(triangle.v[i].y * double(subpixels)));
            /* Over w: a clip distance interpolates perspective-correctly,
             * and its sign along the triangle is that of the screen-space
             * barycentrics weighting each distance over its w. */
            clip_[0][i] = triangle.v[i].clip0 / triangle.v[i].w;
            clip_[1][i] = triangle.v[i].clip1 / triangle.v[i].w;
        }
        const std::int64_t area = (x_[1] - x_[0]) * (y_[2] - y_[0]) - (y_[1] - y_[0]) * (x_[2] - x_[0]);
        if (area == 0) return;
        if (area < 0) {
            swap(1u, 2u);
        }
        valid_ = true;
    }

    std::uint32_t coverage(std::uint32_t px, std::uint32_t py) const
    {
        if (!valid_) return 0u;
        std::uint32_t mask = 0u;
        const Sample *const offsets = pattern(samples_);
        for (std::uint32_t s = 0u; s < samples_; s++) {
            const std::int64_t sx = std::int64_t(px) * subpixels + subpixels / 2 + offsets[s].x * (subpixels / 16);
            const std::int64_t sy = std::int64_t(py) * subpixels + subpixels / 2 + offsets[s].y * (subpixels / 16);
            if (covers(sx, sy)) mask |= 1u << s;
        }
        return mask;
    }

private:
    void swap(std::uint32_t a, std::uint32_t b)
    {
        std::int64_t t = x_[a];
        x_[a] = x_[b];
        x_[b] = t;
        t = y_[a];
        y_[a] = y_[b];
        y_[b] = t;
        for (double(&plane)[3] : clip_) {
            const double d = plane[a];
            plane[a] = plane[b];
            plane[b] = d;
        }
    }

    bool covers(std::int64_t sx, std::int64_t sy) const
    {
        std::int64_t edge[3];
        for (std::uint32_t e = 0u; e < 3u; e++) {
            const std::uint32_t b = (e + 1u) % 3u;
            const std::int64_t dx = x_[b] - x_[e];
            const std::int64_t dy = y_[b] - y_[e];
            edge[e] = dx * (sy - y_[e]) - dy * (sx - x_[e]);
            const bool top_left = dy < 0 || (dy == 0 && dx > 0);
            if (edge[e] < 0 || (edge[e] == 0 && !top_left)) return false;
        }
        /* The weight of a vertex is the edge function of the edge facing it. */
        const double total = double(edge[0] + edge[1] + edge[2]);
        for (const double(&plane)[3] : clip_) {
            const double d = (double(edge[1]) * plane[0] + double(edge[2]) * plane[1] + double(edge[0]) * plane[2]) / total;
            if (d < 0.0) return false;
        }
        return true;
    }

    std::uint32_t samples_;
    bool          valid_;
    std::int64_t  x_[3];
    std::int64_t  y_[3];
    double        clip_[2][3];
};

/* A set of triangles drawn in one call, and what D3D makes of it. */
class Scene {
public:
    Scene() : count_(0u) {}

    void add(const Vertex &a, const Vertex &b, const Vertex &c)
    {
        CHECK(count_ < max_triangles);
        if (count_ < max_triangles) triangles_[count_++] = Triangle{{a, b, c}};
    }

    /* A quad as two triangles sharing the diagonal from (x0, y0) to (x1, y1). */
    void quad(double x0, double y0, double x1, double y1)
    {
        add(point(x0, y0), point(x1, y0), point(x1, y1));
        add(point(x0, y0), point(x1, y1), point(x0, y1));
    }

    static Vertex point(double x, double y) { return Vertex{x, y, 1.0, 1.0, 1.0}; }

    std::uint32_t count() const { return count_; }
    const Triangle &triangle(std::uint32_t index) const { return triangles_[index]; }

    /* Every pixel's value after drawing the scene onto `clear`: the masks
     * combined by `combine` (OR or XOR), or with `write` the last covering
     * triangle's mask, as a target without a logic op takes it. */
    enum class Combine { logic_or, logic_xor, write };

    void expect(std::uint32_t samples, Combine combine, std::uint32_t clear, std::uint32_t *pixels) const
    {
        for (std::uint32_t i = 0u; i < size * size; i++) pixels[i] = clear;
        for (std::uint32_t t = 0u; t < count_; t++) {
            const ReferenceTriangle reference(triangles_[t], samples);
            for (std::uint32_t y = 0u; y < size; y++) {
                for (std::uint32_t x = 0u; x < size; x++) {
                    const std::uint32_t mask = reference.coverage(x, y);
                    if (mask == 0u) continue;
                    std::uint32_t &pixel = pixels[y * size + x];
                    pixel = combine == Combine::logic_or ? pixel | mask
                          : combine == Combine::logic_xor ? pixel ^ mask
                          : mask;
                }
            }
        }
    }

private:
    Triangle      triangles_[max_triangles];
    std::uint32_t count_;
};

/* Hairlines, as Direct2D strokes them: quads a fraction of a pixel thin,
 * horizontal, vertical and slanted, at offsets that put their edges on
 * sample rows and columns of every pattern and between them. */
Scene hairlines()
{
    Scene scene;
    for (std::uint32_t i = 0u; i < 8u; i++) {
        const double y = 2.0 + 3.0 * i + double(i) / 16.0;
        const double width = double(1u + i) / 32.0;
        scene.quad(1.0 + i / 8.0, y, 62.5, y + width);
        const double x = 2.0 + 3.0 * i + double(i) / 16.0 + 1.0 / 32.0;
        scene.quad(x, 26.0, x + width, 62.0 - i / 4.0);
    }
    /* Slanted strokes, one sixteenth of a pixel across. */
    for (std::uint32_t i = 0u; i < 4u; i++) {
        const double x0 = 28.0 + 8.0 * i, y0 = 27.0 + i / 16.0;
        const double x1 = 36.0 + 6.0 * i, y1 = 62.0;
        scene.add(Scene::point(x0, y0), Scene::point(x1, y1), Scene::point(x1 + 1.0 / 16.0, y1));
        scene.add(Scene::point(x0, y0), Scene::point(x1 + 1.0 / 16.0, y1), Scene::point(x0 + 1.0 / 16.0, y0));
    }
    return scene;
}

/* A region tessellated into triangles that share their edges, with grid
 * lines through sample positions: drawn with XOR, a sample counted by two
 * triangles cancels, and one counted by none is missing. */
Scene tiling()
{
    Scene scene;
    constexpr std::uint32_t columns = 6u, rows = 6u;
    Vertex grid[rows + 1u][columns + 1u];
    std::uint32_t seed = 12345u;
    for (std::uint32_t r = 0u; r <= rows; r++) {
        for (std::uint32_t c = 0u; c <= columns; c++) {
            seed = seed * 1664525u + 1013904223u;
            /* Jitter in subpixels on the inside, so edges cross the patterns
             * at every angle; the grid's own lines at x.5 + n/16 and y.5 +
             * n/16 run through sample positions. */
            const bool inside = r > 0u && r < rows && c > 0u && c < columns;
            const double jx = inside ? double(int(seed >> 8u & 0x1ffu) - 256) / 256.0 : 0.0;
            const double jy = inside ? double(int(seed >> 20u & 0x1ffu) - 256) / 256.0 : 0.0;
            grid[r][c] = Scene::point(4.5 + 9.0 * c + 3.0 / 16.0 + jx, 4.5 + 9.0 * r - 5.0 / 16.0 + jy);
        }
    }
    for (std::uint32_t r = 0u; r < rows; r++) {
        for (std::uint32_t c = 0u; c < columns; c++) {
            const Vertex &a = grid[r][c], &b = grid[r][c + 1u], &d = grid[r + 1u][c], &e = grid[r + 1u][c + 1u];
            if ((r + c) % 2u == 0u) {
                scene.add(a, b, e);
                scene.add(a, e, d);
            } else {
                scene.add(a, b, d);
                scene.add(b, e, d);
            }
        }
    }
    return scene;
}

/* Large triangles clipped by both clip distances: x - 20.03 and a slanted
 * plane, neither through any sample position (both lie on the 1/64 pixel
 * grid, off the patterns' 1/16), so the answer is the same at any
 * precision. */
Scene clipped()
{
    Scene scene;
    const auto at = [](double x, double y) {
        return Vertex{x, y, x - 20.0 - 1.0 / 32.0, 40.0 + 1.0 / 64.0 - 0.5 * x - y, 1.0};
    };
    scene.add(at(2.0, 3.0), at(61.0, 9.0), at(30.0, 60.0));
    scene.add(at(5.0, 50.0), at(55.0, 62.0), at(40.0, 35.0));
    return scene;
}

/* The tiling and the clipped triangles in perspective: each vertex at its
 * own w (1, 2 or 4, so that x / w stays exact), and the clip distances
 * scaled by it, which keeps their planes where they were on screen only if
 * the distances are divided by w where they are interpolated. */
Scene perspective()
{
    Scene scene;
    const Scene sources[2] = {tiling(), clipped()};
    std::uint32_t index = 0u;
    for (const Scene &source : sources) {
        for (std::uint32_t t = 0u; t < source.count(); t++) {
            Vertex v[3];
            for (std::uint32_t i = 0u; i < 3u; i++) {
                v[i] = source.triangle(t).v[i];
                v[i].w = double(1u << (index++ % 3u));
                v[i].clip0 *= v[i].w;
                v[i].clip1 *= v[i].w;
            }
            scene.add(v[0], v[1], v[2]);
        }
    }
    return scene;
}

/* Triangles too thin to cover a sample in most pixels they cross, apart from
 * each other: drawn with no logic op onto a cleared target, a pixel shader
 * run where no sample is covered would overwrite the clear with zero. */
Scene slivers()
{
    Scene scene;
    for (std::uint32_t i = 0u; i < 6u; i++) {
        const double x = 4.0 + 10.0 * i + double(i) / 64.0;
        scene.add(Scene::point(x, 2.0), Scene::point(x + 7.0, 61.0), Scene::point(x + 3.0 / 32.0, 2.0));
    }
    return scene;
}

/* One device on one GPU with everything the scenes draw with: a 64x64
 * R32_UINT target and a UAV texture like it, the shaders, and the states. */
class TirRig {
public:
    explicit TirRig(av::D3dLoopback &wire) : wire_(wire) {}

    void make()
    {
        ResourceDesc texture = {};
        texture.dimension = 3u;
        texture.width = size;
        texture.height = size;
        texture.depth_or_array = 1u;
        texture.mip_levels = 1u;
        texture.format = format_r32_uint;
        texture.sample_count = 1u;
        texture.bind_flags = bind_render_target;
        CHECK(av_d3d_CreateResource(wire_, target_id, texture, 0u));
        texture.bind_flags = bind_unordered_access;
        CHECK(av_d3d_CreateResource(wire_, uav_texture_id, texture, 0u));
        ViewDesc view = {};
        view.resource = target_id;
        view.format = format_r32_uint;
        view.dimension = view_texture2d;
        CHECK(av_d3d_CreateRenderTargetView(wire_, target_view_id, view));
        view.resource = uav_texture_id;
        CHECK(av_d3d_CreateUnorderedAccessView(wire_, uav_view_id, view));

        ResourceDesc buffer = {};
        buffer.dimension = 1u;
        buffer.width = sizeof(WireVertex) * 3u * max_triangles;
        buffer.bind_flags = bind_vertex;
        CHECK(av_d3d_CreateResource(wire_, vertices_id, buffer, 0u));

        CHECK(av_d3d_CreateShader(wire_, vs_id, 0u, tir_vs::inputs_count, tir_vs::inputs, tir_vs::outputs_count,
                                   tir_vs::outputs, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0xffffffffu, tir_vs::tokens,
                                   sizeof(tir_vs::tokens)));
        CHECK(av_d3d_CreateShader(wire_, coverage_ps_id, 4u, tir_coverage_ps::inputs_count, tir_coverage_ps::inputs,
                                   tir_coverage_ps::outputs_count, tir_coverage_ps::outputs, 0u, nullptr, 0u, nullptr,
                                   0u, nullptr, 0xffffffffu, tir_coverage_ps::tokens,
                                   sizeof(tir_coverage_ps::tokens)));
        CHECK(av_d3d_CreateShader(wire_, uav_ps_id, 4u, tir_uav_ps::inputs_count, tir_uav_ps::inputs,
                                   tir_uav_ps::outputs_count, tir_uav_ps::outputs, 0u, nullptr, 0u, nullptr, 0u,
                                   nullptr, 0xffffffffu, tir_uav_ps::tokens, sizeof(tir_uav_ps::tokens)));
        CHECK(av_d3d_CreateShader(wire_, clip_ps_id, 4u, tir_clip_ps::inputs_count, tir_clip_ps::inputs,
                                   tir_clip_ps::outputs_count, tir_clip_ps::outputs, 0u, nullptr, 0u, nullptr, 0u,
                                   nullptr, 0xffffffffu, tir_clip_ps::tokens, sizeof(tir_clip_ps::tokens)));
        CHECK(av_d3d_CreateShader(wire_, attribute_vs_id, 0u, tir_attribute_vs::inputs_count, tir_attribute_vs::inputs,
                                   tir_attribute_vs::outputs_count, tir_attribute_vs::outputs, 0u, nullptr, 0u,
                                   nullptr, 0u, nullptr, 0xffffffffu, tir_attribute_vs::tokens,
                                   sizeof(tir_attribute_vs::tokens)));
        CHECK(av_d3d_CreateShader(wire_, eval_ps_id, 4u, tir_eval_ps::inputs_count, tir_eval_ps::inputs,
                                   tir_eval_ps::outputs_count, tir_eval_ps::outputs, 0u, nullptr, 0u, nullptr, 0u,
                                   nullptr, 0xffffffffu, tir_eval_ps::tokens, sizeof(tir_eval_ps::tokens)));
        const InputElement elements[2] = {{0u, 0u, format_r32g32b32a32, 0u, 0u, 0u},
                                          {0u, 16u, format_r32g32, 0u, 0u, 1u}};
        CHECK(av_d3d_CreateInputLayout(wire_, layout_id, 2u, elements));
        const InputElement position[1] = {{0u, 0u, format_r32g32, 0u, 0u, 0u}};
        CHECK(av_d3d_CreateInputLayout(wire_, attribute_layout_id, 1u, position));

        make_blend(blend_or_id, true, logic_op_or);
        make_blend(blend_xor_id, true, logic_op_xor);
        make_blend(blend_write_id, false, 0u);

        make_rasterizer(rasterizer_normal_id, 0u);
        for (std::uint32_t samples : forced_counts) make_rasterizer(rasterizer_id(samples), samples);
        CHECK_EQ(wire_.refused(), 0u);
    }

    /* `scene` drawn in one call at `samples` (0: not forced) into the target
     * cleared to `clear`, with `blend`, read back into `pixels`. */
    void draw_to_target(const Scene &scene, std::uint32_t samples, std::uint32_t blend, std::uint32_t clear,
                        std::uint32_t *pixels, std::uint32_t ps = coverage_ps_id)
    {
        const std::uint32_t targets[1] = {target_view_id};
        CHECK(av_d3d_SetRenderTargets(wire_, 1u, targets, 0u, 1u, 0u, nullptr));
        CHECK(av_d3d_ClearRenderTargetView(wire_, target_view_id, Float4{{float(clear), 0.0f, 0.0f, 0.0f}}));
        CHECK(av_d3d_SetShader(wire_, 4u, ps));
        CHECK(av_d3d_SetBlendState(wire_, blend, Float4{{0.0f, 0.0f, 0.0f, 0.0f}}, 0xffffffffu));
        draw(scene, samples);
        read(target_id, pixels);
    }

    /* `scene` drawn at `samples` by the attribute shaders, the target
     * cleared to zero and the values OR'd in. */
    void draw_attribute(const Scene &scene, std::uint32_t samples, std::uint32_t *pixels)
    {
        const std::uint32_t targets[1] = {target_view_id};
        CHECK(av_d3d_SetRenderTargets(wire_, 1u, targets, 0u, 1u, 0u, nullptr));
        CHECK(av_d3d_ClearRenderTargetView(wire_, target_view_id, Float4{{0.0f, 0.0f, 0.0f, 0.0f}}));
        CHECK(av_d3d_SetShader(wire_, 4u, eval_ps_id));
        CHECK(av_d3d_SetBlendState(wire_, blend_or_id, Float4{{0.0f, 0.0f, 0.0f, 0.0f}}, 0xffffffffu));
        draw(scene, samples, attribute_vs_id, attribute_layout_id);
        read(target_id, pixels);
    }

    /* `scene` drawn at `samples` with only the UAV bound, its pixel shader
     * OR'ing the coverage into the UAV cleared to zero. */
    void draw_to_uav(const Scene &scene, std::uint32_t samples, std::uint32_t *pixels)
    {
        const UavBinding uav = {uav_view_id, 0xffffffffu};
        CHECK(av_d3d_SetRenderTargets(wire_, 0u, nullptr, 0u, 0u, 1u, &uav));
        CHECK(av_d3d_ClearUnorderedAccessViewUint(wire_, uav_view_id, Uint4{{0u, 0u, 0u, 0u}}));
        CHECK(av_d3d_SetShader(wire_, 4u, uav_ps_id));
        draw(scene, samples);
        read(uav_texture_id, pixels);
    }

private:
    static std::uint32_t rasterizer_id(std::uint32_t samples)
    {
        return samples == 0u ? rasterizer_normal_id : rasterizer_forced_base_id + samples;
    }

    void make_blend(std::uint32_t id, bool logic, std::uint32_t op)
    {
        BlendDesc desc = {};
        for (RenderTargetBlend &target : desc.targets) {
            target.logic_op_enable = logic ? 1u : 0u;
            target.logic_op = op;
            target.src_blend = blend_one;
            target.dest_blend = blend_zero;
            target.blend_op = blend_op_add;
            target.src_blend_alpha = blend_one;
            target.dest_blend_alpha = blend_zero;
            target.blend_op_alpha = blend_op_add;
            target.write_mask = 0xfu;
        }
        CHECK(av_d3d_CreateBlendState(wire_, id, desc));
    }

    void make_rasterizer(std::uint32_t id, std::uint32_t samples)
    {
        RasterizerDesc desc = {};
        desc.fill_mode = fill_solid;
        desc.cull_mode = cull_none;
        desc.depth_clip = 1u;
        desc.forced_sample_count = samples;
        CHECK(av_d3d_CreateRasterizerState(wire_, id, desc));
    }

    void draw(const Scene &scene, std::uint32_t samples, std::uint32_t vs = vs_id, std::uint32_t layout = layout_id)
    {
        static WireVertex vertices[3u * max_triangles];
        static float      positions[3u * max_triangles][2];
        const bool        attribute = vs == attribute_vs_id;
        for (std::uint32_t t = 0u; t < scene.count(); t++) {
            for (std::uint32_t i = 0u; i < 3u; i++) {
                const Vertex &v = scene.triangle(t).v[i];
                /* Clip space for a 64x64 viewport: exact for any vertex on
                 * the subpixel grid, and w a power of two. */
                const float x = float(v.x / 32.0 - 1.0), y = float(1.0 - v.y / 32.0), w = float(v.w);
                vertices[3u * t + i] = WireVertex{x * w, y * w, 0.0f, w, float(v.clip0), float(v.clip1)};
                positions[3u * t + i][0] = x;
                positions[3u * t + i][1] = y;
            }
        }
        const std::uint32_t stride = attribute ? std::uint32_t(sizeof(positions[0])) : std::uint32_t(sizeof(WireVertex));
        const std::uint32_t bytes = stride * 3u * scene.count();
        const Box written = {0u, 0u, 0u, bytes, 1u, 1u};
        CHECK(av_d3d_UpdateSubresource(wire_, vertices_id, 0u, 1u, written, 0u, 0u, 0u,
                                       attribute ? static_cast<const void *>(positions) : vertices, bytes));
        const Viewport viewport = {0.0f, 0.0f, float(size), float(size), 0.0f, 1.0f};
        CHECK(av_d3d_SetViewports(wire_, 1u, &viewport));
        CHECK(av_d3d_SetInputLayout(wire_, layout));
        const VertexBufferBinding binding = {vertices_id, stride, 0u};
        CHECK(av_d3d_SetVertexBuffers(wire_, 0u, 1u, &binding));
        CHECK(av_d3d_SetTopology(wire_, topology_triangles));
        CHECK(av_d3d_SetShader(wire_, 0u, vs));
        CHECK(av_d3d_SetRasterizerState(wire_, rasterizer_id(samples)));
        CHECK(av_d3d_Draw(wire_, 3u * scene.count(), 0u));
    }

    void read(std::uint32_t resource, std::uint32_t *pixels)
    {
        std::uint32_t hresult = 1u, row_pitch = 0u, depth_pitch = 0u;
        std::uint64_t bytes = 0u;
        CHECK(av_d3d_ReadSubresource(wire_, resource, 0u, &hresult, &row_pitch, &depth_pitch, pixels,
                                     size * size * sizeof(std::uint32_t), &bytes));
        CHECK_EQ(hresult, 0u);
        CHECK_EQ(row_pitch, size * 4u);
        CHECK_EQ(wire_.refused(), 0u);
    }

    av::D3dLoopback &wire_;
};

/* How many pixels differ, the first few printed. */
std::uint32_t mismatches(const char *what, std::uint32_t samples, const std::uint32_t *actual,
                         const std::uint32_t *expected)
{
    std::uint32_t count = 0u;
    for (std::uint32_t i = 0u; i < size * size; i++) {
        if (actual[i] == expected[i]) continue;
        if (count++ < 4u)
            std::fprintf(stderr, "%s at %u samples: pixel (%u, %u) is 0x%08x, D3D's 0x%08x\n", what, samples, i % size,
                         i / size, actual[i], expected[i]);
    }
    return count;
}

/* A backend is several megabytes, so it lives in static storage. */
av::ObjectStorage<av::D3dBackend> backend_storage;

/* `judge` run against a device on every GPU of this host that runs DXVK. */
template <typename Judge>
void on_every_gpu(Judge judge)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::LoaderHostApis apis;
    apis.open();
    std::uint32_t judged = 0u;
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++) {
        av::D3d11HostDevice *const device = apis.d3d11_device(index);
        if (device == nullptr) continue;
        CHECK_STATUS(backend_storage.construct(), av::Status::ok);
        av::D3dBackend &backend = backend_storage.object();
        av::SessionShares unshared;
        CHECK(backend.initialize(unshared, device));
        {
            av::D3dLoopback wire(backend);
            std::uint32_t hresult = 1u, level = 0u;
            CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
            CHECK_EQ(hresult, 0u);
            /* Target-independent rasterization is feature level 11_1's. */
            CHECK(level >= 0xb100u);
            TirRig rig(wire);
            rig.make();
            judge(rig);
            CHECK_EQ(wire.refused(), 0u);
            CHECK(backend.invariants_hold());
        }
        backend.shut_down();
        backend_storage.destroy();
        judged++;
    }
    if (judged == 0u) std::fprintf(stderr, "no GPU here runs DXVK: nothing judged\n");
}

std::uint32_t actual[size * size];
std::uint32_t expected[size * size];

}  // namespace

/* Direct2D's hairlines at every forced count, OR'd into a single-sample
 * target: every pixel's mask is the D3D pattern's, bit for bit. Before the
 * emulation, rows of these strokes went missing - a 16-sample state drawn
 * at one sample. */
TEST(d3d_tir_device, hairlines_or_into_a_single_sample_target_cover_the_forced_pattern)
{
    const Scene scene = hairlines();
    on_every_gpu([&](TirRig &rig) {
        for (std::uint32_t samples : forced_counts) {
            rig.draw_to_target(scene, samples, blend_or_id, 0u, actual);
            scene.expect(samples, Scene::Combine::logic_or, 0u, expected);
            CHECK_EQ(mismatches("hairlines", samples, actual, expected), 0u);
        }
    });
}

/* Triangles sharing edges, XOR'd: each sample on a shared edge belongs to
 * exactly one of the two triangles (the top-left rule), so the target holds
 * the union's coverage and no bit cancels. */
TEST(d3d_tir_device, a_tiling_drawn_with_xor_counts_every_sample_on_a_shared_edge_once)
{
    const Scene scene = tiling();
    on_every_gpu([&](TirRig &rig) {
        for (std::uint32_t samples : multisample_counts) {
            rig.draw_to_target(scene, samples, blend_xor_id, 0u, actual);
            scene.expect(samples, Scene::Combine::logic_xor, 0u, expected);
            CHECK_EQ(mismatches("tiling", samples, actual, expected), 0u);
            /* The union is solid inside: XOR and OR agree there. */
            CHECK_EQ(actual[30u * size + 30u], (1u << samples) - 1u);
        }
    });
}

/* Clip distances clip at every sample of the forced pattern, not at the
 * pixel the emulation rasterizes. */
TEST(d3d_tir_device, clip_distances_clip_each_sample_of_the_forced_pattern)
{
    const Scene scene = clipped();
    on_every_gpu([&](TirRig &rig) {
        for (std::uint32_t samples : high_counts) {
            rig.draw_to_target(scene, samples, blend_or_id, 0u, actual);
            scene.expect(samples, Scene::Combine::logic_or, 0u, expected);
            CHECK_EQ(mismatches("clipped", samples, actual, expected), 0u);
        }
    });
}

/* A pixel the triangle touches without covering any sample of the pattern
 * runs no pixel shader: with no logic op, one that ran would write a zero
 * mask over the clear. */
TEST(d3d_tir_device, a_pixel_with_no_covered_sample_runs_no_pixel_shader)
{
    const Scene scene = slivers();
    on_every_gpu([&](TirRig &rig) {
        rig.draw_to_target(scene, 16u, blend_write_id, untouched, actual);
        scene.expect(16u, Scene::Combine::write, untouched, expected);
        CHECK_EQ(mismatches("slivers", 16u, actual, expected), 0u);
        std::uint32_t written = 0u;
        for (std::uint32_t i = 0u; i < size * size; i++) written += actual[i] != untouched ? 1u : 0u;
        CHECK(written > 0u);
    });
}

/* Without a render target (UAV-only rendering) the forced pattern is the
 * same: emulated too, though Vulkan could rasterize up to 8 samples with no
 * attachments here - its hardware clips clip distances geometrically, which
 * moved an edge past a sample of this scene on RADV, where D3D clips each
 * sample. */
TEST(d3d_tir_device, uav_only_rendering_covers_the_forced_pattern_as_a_render_target_does)
{
    const Scene lines = hairlines();
    const Scene clips = clipped();
    on_every_gpu([&](TirRig &rig) {
        for (std::uint32_t samples : multisample_counts) {
            rig.draw_to_uav(lines, samples, actual);
            lines.expect(samples, Scene::Combine::logic_or, 0u, expected);
            CHECK_EQ(mismatches("uav hairlines", samples, actual, expected), 0u);
            rig.draw_to_uav(clips, samples, actual);
            clips.expect(samples, Scene::Combine::logic_or, 0u, expected);
            CHECK_EQ(mismatches("uav clipped", samples, actual, expected), 0u);
        }
    });
}

/* After an emulated draw the next one without a forced count is drawn as
 * before, at one sample, and the one after that is emulated again. */
TEST(d3d_tir_device, a_draw_without_a_forced_count_after_an_emulated_one_rasterizes_at_one_sample)
{
    const Scene scene = tiling();
    on_every_gpu([&](TirRig &rig) {
        rig.draw_to_target(scene, 16u, blend_or_id, 0u, actual);
        CHECK_EQ(actual[30u * size + 30u], 0xffffu);
        rig.draw_to_target(scene, 0u, blend_or_id, 0u, actual);
        std::uint32_t covered = 0u;
        for (std::uint32_t i = 0u; i < size * size; i++) {
            CHECK(actual[i] <= 1u);
            covered += actual[i];
        }
        /* The tiling spans 54x54 pixels around its jitter. */
        CHECK(covered > 2700u && covered < 3100u);
        rig.draw_to_target(scene, 16u, blend_xor_id, 0u, actual);
        scene.expect(16u, Scene::Combine::logic_xor, 0u, expected);
        CHECK_EQ(mismatches("tiling again", 16u, actual, expected), 0u);
    });
}

/* Pull-model interpolation by sample index evaluates at the forced pattern's
 * sample, not at the one sample the emulation rasterizes: sample 12 of 16 is
 * half a pixel left of the center, so each covered pixel gets its left edge,
 * in sixteenths of a pixel. */
TEST(d3d_tir_device, interpolation_by_sample_index_reads_the_forced_pattern)
{
    Scene scene;
    scene.quad(4.0, 4.0, 60.0, 60.0);
    on_every_gpu([&](TirRig &rig) {
        rig.draw_attribute(scene, 16u, actual);
        std::uint32_t wrong = 0u;
        for (std::uint32_t y = 0u; y < size; y++) {
            for (std::uint32_t x = 0u; x < size; x++) {
                const bool inside = x >= 4u && x < 60u && y >= 4u && y < 60u;
                wrong += actual[y * size + x] == (inside ? 16u * x : 0u) ? 0u : 1u;
            }
        }
        CHECK_EQ(wrong, 0u);
    });
}

/* Perspective: window coordinates divide by each vertex's own w before they
 * are snapped, and clip distances are interpolated perspective-correctly. */
TEST(d3d_tir_device, triangles_in_perspective_snap_and_clip_as_d3d_projects_them)
{
    const Scene scene = perspective();
    on_every_gpu([&](TirRig &rig) {
        for (std::uint32_t samples : high_counts) {
            rig.draw_to_target(scene, samples, blend_or_id, 0u, actual);
            scene.expect(samples, Scene::Combine::logic_or, 0u, expected);
            CHECK_EQ(mismatches("perspective", samples, actual, expected), 0u);
        }
    });
}

/* A pixel shader reading SV_ClipDistance gets it from the stage the
 * emulation puts before it, and the coverage is clipped all the same. */
TEST(d3d_tir_device, a_pixel_shader_reading_clip_distances_still_sees_the_forced_pattern)
{
    const Scene scene = clipped();
    on_every_gpu([&](TirRig &rig) {
        rig.draw_to_target(scene, 16u, blend_or_id, 0u, actual, clip_ps_id);
        scene.expect(16u, Scene::Combine::logic_or, 0u, expected);
        CHECK_EQ(mismatches("clip reader", 16u, actual, expected), 0u);
    });
}
