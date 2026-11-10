#include "executor/d3d/d3d9_line_ends.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {

constexpr uint32_t last_pixel_state = D3DRS_LASTPIXEL;

}

class D3d9LineEnds::SinglePixelPoints {
public:
    explicit SinglePixelPoints(IDirect3DDevice9 &device);
    ~SinglePixelPoints();
    SinglePixelPoints(const SinglePixelPoints &) = delete;
    SinglePixelPoints &operator=(const SinglePixelPoints &) = delete;

private:
    static constexpr uint32_t state_count = 3u;

    IDirect3DDevice9 &device_;
    DWORD             kept_[state_count];
};

namespace {

constexpr D3DRENDERSTATETYPE point_states[3] = {D3DRS_POINTSIZE, D3DRS_POINTSPRITEENABLE, D3DRS_POINTSCALEENABLE};
constexpr float              one_pixel = 1.0f;

}

D3d9LineEnds::SinglePixelPoints::SinglePixelPoints(IDirect3DDevice9 &device) : device_(device), kept_{}
{
    AV_SCOPE("D3d9LineEnds::SinglePixelPoints::SinglePixelPoints");
    for (uint32_t index = 0u; index < state_count; index++) static_cast<void>(device_.GetRenderState(point_states[index], &kept_[index]));
    DWORD size = 0u;
    __builtin_memcpy(&size, &one_pixel, sizeof(size));
    static_cast<void>(device_.SetRenderState(D3DRS_POINTSIZE, size));
    static_cast<void>(device_.SetRenderState(D3DRS_POINTSPRITEENABLE, FALSE));
    static_cast<void>(device_.SetRenderState(D3DRS_POINTSCALEENABLE, FALSE));
}

D3d9LineEnds::SinglePixelPoints::~SinglePixelPoints()
{
    AV_SCOPE("D3d9LineEnds::SinglePixelPoints::~SinglePixelPoints");
    for (uint32_t index = 0u; index < state_count; index++) static_cast<void>(device_.SetRenderState(point_states[index], kept_[index]));
}

D3d9LineEnds::D3d9LineEnds() : last_pixel_(true)
{
    AV_SCOPE("D3d9LineEnds::D3d9LineEnds");
}

void D3d9LineEnds::render_state(uint32_t state, uint32_t value)
{
    AV_SCOPE("D3d9LineEnds::render_state");
    if (state == last_pixel_state) last_pixel_ = value != 0u;
}

bool D3d9LineEnds::wanted(uint32_t type) const
{
    AV_SCOPE("D3d9LineEnds::wanted");
    return last_pixel_ && (type == D3DPT_LINELIST || type == D3DPT_LINESTRIP);
}

void D3d9LineEnds::draw(IDirect3DDevice9 &device, uint32_t type, uint32_t start_vertex, uint32_t primitive_count) const
{
    AV_SCOPE("D3d9LineEnds::draw");
    if (!wanted(type)) return;
    const SinglePixelPoints points(device);
    if (type == D3DPT_LINESTRIP) {
        static_cast<void>(device.DrawPrimitive(D3DPT_POINTLIST, start_vertex + primitive_count, 1u));
        return;
    }
    for (uint32_t line = 0u; line < primitive_count; line++)
        static_cast<void>(device.DrawPrimitive(D3DPT_POINTLIST, start_vertex + 2u * line + 1u, 1u));
}

void D3d9LineEnds::draw_indexed(IDirect3DDevice9 &device, uint32_t type, int32_t base_vertex, uint32_t min_index,
                                uint32_t vertex_count, uint32_t start_index, uint32_t primitive_count) const
{
    AV_SCOPE("D3d9LineEnds::draw_indexed");
    if (!wanted(type)) return;
    const SinglePixelPoints points(device);
    if (type == D3DPT_LINESTRIP) {
        static_cast<void>(device.DrawIndexedPrimitive(D3DPT_POINTLIST, base_vertex, min_index, vertex_count,
                                                      start_index + primitive_count, 1u));
        return;
    }
    for (uint32_t line = 0u; line < primitive_count; line++)
        static_cast<void>(device.DrawIndexedPrimitive(D3DPT_POINTLIST, base_vertex, min_index, vertex_count,
                                                      start_index + 2u * line + 1u, 1u));
}

}
