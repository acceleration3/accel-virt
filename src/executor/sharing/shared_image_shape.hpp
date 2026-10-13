/* What a shared object's memory is as an image, as the session that made it
 * says: a Direct3D 11 texture shared over exportable memory is one 2D image,
 * `width` by `height` texels of a DXGI format, its rows `pitch` bytes apart
 * as the GPU laid the linear image out. An importer that opens the memory
 * views it as that image - OpenCL as an image over the buffer it imports -
 * rather than as bytes. Never taken from the guest: it is the publisher's own
 * description of what it made. Empty for memory that is no image. */
#ifndef AV_SHARED_IMAGE_SHAPE_HPP
#define AV_SHARED_IMAGE_SHAPE_HPP

#include <stdint.h>

namespace av {

class SharedImageShape {
public:
    SharedImageShape();
    SharedImageShape(uint32_t width, uint32_t height, uint32_t format, uint32_t pitch);

    /* Whether it describes an image at all: every field non-zero. */
    bool     described() const;
    uint32_t width() const;
    uint32_t height() const;
    /* The DXGI_FORMAT. */
    uint32_t format() const;
    uint32_t pitch() const;

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t format_;
    uint32_t pitch_;
};

}  // namespace av

#endif  // AV_SHARED_IMAGE_SHAPE_HPP
