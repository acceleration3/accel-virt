#ifndef AV_IMAGE_SINK_HPP
#define AV_IMAGE_SINK_HPP

#include "guest/probe/bmp_image.hpp"

#include <stddef.h>

namespace av {

class ImageSink {
public:
    static constexpr size_t max_bytes = BmpImage::header_bytes + 128u * 128u * 3u;
    static constexpr size_t max_name = 32u;

    ImageSink() = default;
    ImageSink(const ImageSink &) = delete;
    ImageSink &operator=(const ImageSink &) = delete;

    virtual void save(const char *name, const BmpImage &image) = 0;

protected:
    ~ImageSink() = default;
};

}

#endif
