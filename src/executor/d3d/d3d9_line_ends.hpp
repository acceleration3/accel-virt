#ifndef AV_D3D9_LINE_ENDS_HPP
#define AV_D3D9_LINE_ENDS_HPP

#include <stdint.h>

struct IDirect3DDevice9;

namespace av {

class D3d9LineEnds {
public:
    D3d9LineEnds();
    D3d9LineEnds(const D3d9LineEnds &) = delete;
    D3d9LineEnds &operator=(const D3d9LineEnds &) = delete;

    void render_state(uint32_t state, uint32_t value);
    bool wanted(uint32_t type) const;
    void draw(IDirect3DDevice9 &device, uint32_t type, uint32_t start_vertex, uint32_t primitive_count) const;
    void draw_indexed(IDirect3DDevice9 &device, uint32_t type, int32_t base_vertex, uint32_t min_index,
                      uint32_t vertex_count, uint32_t start_index, uint32_t primitive_count) const;

private:
    class SinglePixelPoints;

    bool last_pixel_;
};

}

#endif
