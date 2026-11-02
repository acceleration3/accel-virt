#ifndef AV_SCRIPTED_IMAGE_SINK_HPP
#define AV_SCRIPTED_IMAGE_SINK_HPP

#include "guest/probe/image_sink.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ScriptedImageSink final : public ImageSink {
public:
    static constexpr std::size_t max_images = 4u;

    ScriptedImageSink();

    void save(const char *name, const BmpImage &image) override;

    std::size_t          saved() const;
    const std::uint8_t *bytes(const char *name) const;
    std::size_t          size(const char *name) const;

private:
    struct Kept {
        char         name[max_name];
        std::uint8_t bytes[max_bytes];
        std::size_t  size;
    };
    const Kept *find(const char *name) const;

    Kept        kept_[max_images];
    std::size_t saved_;
};

}

#endif
