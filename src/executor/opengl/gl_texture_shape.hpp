#ifndef AV_GL_TEXTURE_SHAPE_HPP
#define AV_GL_TEXTURE_SHAPE_HPP

#include <stdint.h>

namespace av {

class GlTextureShape {
public:
    static constexpr uint32_t most_levels = 32u;

    GlTextureShape(uint32_t target, uint32_t format, int64_t levels, int64_t samples, int64_t width, int64_t height,
                   int64_t depth);

    bool     tracked() const;
    bool     valid() const;
    uint32_t binding() const;
    uint64_t least_bytes() const;

private:
    struct Format {
        uint32_t format_;
        uint8_t  block_width_;
        uint8_t  block_height_;
        uint8_t  block_bytes_;
    };

    uint64_t level_bytes(uint32_t level, const Format &format) const;
    Format   describe_format() const;
    uint32_t layers() const;

    uint32_t target_;
    uint32_t format_;
    int64_t  levels_;
    int64_t  samples_;
    int64_t  width_;
    int64_t  height_;
    int64_t  depth_;
};

}

#endif
