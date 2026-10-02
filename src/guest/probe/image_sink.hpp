/* Where a probe leaves the pictures of what it drew (docs/testing.md): a file
 * beside the probe in the guest, a directory of the build on Linux, a list a
 * unit test reads. A picture that cannot be kept is not a failed step - the
 * step judges the pixels - so saving answers nothing. */
#ifndef AV_IMAGE_SINK_HPP
#define AV_IMAGE_SINK_HPP

#include "guest/probe/bmp_image.hpp"

#include <stddef.h>

namespace av {

class ImageSink {
public:
    /* A probe's pictures are at most 64x64: the file of one, and a name's
     * longest spelling with its terminator. */
    /* The largest picture a probe keeps: the present step's resized window
     * (feature 046), which is larger than the 64x64 renders. */
    static constexpr size_t max_bytes = BmpImage::header_bytes + 128u * 128u * 3u;
    static constexpr size_t max_name = 32u;

    ImageSink() = default;
    ImageSink(const ImageSink &) = delete;
    ImageSink &operator=(const ImageSink &) = delete;

    /* `name` without a directory or extension, such as "opengl-draw". */
    virtual void save(const char *name, const BmpImage &image) = 0;

protected:
    ~ImageSink() = default;
};

}  // namespace av

#endif  // AV_IMAGE_SINK_HPP
