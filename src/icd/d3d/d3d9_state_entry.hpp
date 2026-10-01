/* One piece of a Direct3D 9 device's state as a state block holds it: what it
 * is (kind and up to two indices) and its value, at most four words - a
 * render state's value, a shader register's four floats, a stream's buffer,
 * offset and stride, a viewport. The DDI's state-setting calls are each one
 * or more of these (D3d9StateShadow, D3d9StateBlock). */
#ifndef AV_ICD_D3D9_STATE_ENTRY_HPP
#define AV_ICD_D3D9_STATE_ENTRY_HPP

#include <stdint.h>

namespace av::d3d {

class D3d9StateEntry {
public:
    enum class Kind : uint8_t {
        none,
        render,          /* a: D3DRENDERSTATETYPE */
        stage,           /* a: stage or sampler number, b: D3DDDITSS_* */
        texture,         /* a: stage or sampler number; value: the resource handle */
        vertex_shader,   /* value: the handle */
        pixel_shader,
        declaration,
        constant_f,      /* a: 0 vertex or 1 pixel, b: register; value: four floats' bits */
        constant_i,      /* the same, four ints */
        constant_b,      /* the same, one BOOL */
        stream,          /* a: stream; value: handle (two words), offset, stride */
        indices,         /* value: handle (two words), stride */
        viewport,        /* value: x, y, width, height */
        z_range,         /* value: min and max as float bits */
        scissor,         /* value: left, top, right, bottom */
        clip_plane,      /* a: index; value: four floats' bits */
    };
    static constexpr uint32_t value_words = 4u;

    D3d9StateEntry();
    D3d9StateEntry(Kind kind, uint32_t a, uint32_t b, const uint32_t *value, uint32_t words);
    /* An entry whose value is a handle - a texture, shader, declaration,
     * stream or index buffer - in the first two words, then `second` and
     * `third`. */
    D3d9StateEntry(Kind kind, uint32_t a, const void *handle, uint32_t second, uint32_t third);

    Kind            kind() const;
    uint32_t        a() const;
    uint32_t        b() const;
    const uint32_t *value() const;
    /* The handle an entry made with one holds. */
    void           *handle() const;
    /* The same piece of state as `other`, whatever its value. */
    bool            names(const D3d9StateEntry &other) const;
    void            take_value(const D3d9StateEntry &other);

private:
    Kind     kind_;
    uint32_t a_;
    uint32_t b_;
    uint32_t value_[value_words];
};

}  // namespace av::d3d

#endif  // AV_ICD_D3D9_STATE_ENTRY_HPP
