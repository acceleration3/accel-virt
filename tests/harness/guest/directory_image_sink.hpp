#ifndef AV_DIRECTORY_IMAGE_SINK_HPP
#define AV_DIRECTORY_IMAGE_SINK_HPP

#include "guest/probe/image_sink.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class DirectoryImageSink final : public ImageSink {
public:
    explicit DirectoryImageSink(const char *directory);

    void save(const char *name, const BmpImage &image) override;

    std::size_t written() const;

private:
    const char  *directory_;
    std::uint8_t bytes_[max_bytes];
    std::size_t  written_;
};

}

#endif
