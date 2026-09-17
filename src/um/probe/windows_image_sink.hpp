/* A probe's pictures as C:\rvk\<name>.bmp, beside the probe, for the runner
 * to pull into the run's directory. */
#ifndef AV_WINDOWS_IMAGE_SINK_HPP
#define AV_WINDOWS_IMAGE_SINK_HPP

#include "guest/suite/image_sink.hpp"

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

}  // namespace av

#endif  // AV_WINDOWS_IMAGE_SINK_HPP
