#ifndef AV_GLICD_FRONT_BUFFER_HPP
#define AV_GLICD_FRONT_BUFFER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::glicd {

struct FrontBufferProbe;

class FrontBuffer {
public:
    enum : uint32_t { gl_rgba = 0x1908u, gl_bgra = 0x80E1u, gl_unsigned_byte = 0x1401u };
    static constexpr uint32_t bytes_per_pixel = 4u;

    FrontBuffer();

    void presented(const uint8_t *bgra_bottom_up, uint32_t width, uint32_t height);
    void forget();
    bool presented() const;
    uint32_t width() const;
    uint32_t height() const;

    [[nodiscard]] bool read(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type,
                            void *pixels) const;

    bool invariants_hold() const;

private:
    const uint8_t *pixels_;
    uint32_t       width_;
    uint32_t       height_;

    friend struct FrontBufferProbe;
};

}

#endif
