#ifndef AV_WINDOWS_IMAGE_SINK_HPP
#define AV_WINDOWS_IMAGE_SINK_HPP

#include "guest/probe/image_sink.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WindowsImageSink final : public ImageSink {
public:
    WindowsImageSink();

    void save(const char *name, const BmpImage &image) override;

private:
    uint8_t bytes_[max_bytes];
};

}

#endif
