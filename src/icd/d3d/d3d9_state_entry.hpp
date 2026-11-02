#ifndef AV_ICD_D3D9_STATE_ENTRY_HPP
#define AV_ICD_D3D9_STATE_ENTRY_HPP

#include <stdint.h>

namespace av::d3d {

class D3d9StateEntry {
public:
    enum class Kind : uint8_t {
        none,
        render,
        stage,
        texture,
        vertex_shader,
        pixel_shader,
        declaration,
        constant_f,
        constant_i,
        constant_b,
        stream,
        indices,
        viewport,
        z_range,
        scissor,
        clip_plane,
    };
    static constexpr uint32_t value_words = 4u;

    D3d9StateEntry();
    D3d9StateEntry(Kind kind, uint32_t a, uint32_t b, const uint32_t *value, uint32_t words);
    D3d9StateEntry(Kind kind, uint32_t a, const void *handle, uint32_t second, uint32_t third);

    Kind            kind() const;
    uint32_t        a() const;
    uint32_t        b() const;
    const uint32_t *value() const;
    void           *handle() const;
    bool            names(const D3d9StateEntry &other) const;
    void            take_value(const D3d9StateEntry &other);

private:
    Kind     kind_;
    uint32_t a_;
    uint32_t b_;
    uint32_t value_[value_words];
};

}

#endif
