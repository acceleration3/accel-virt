/* An ImageSink a unit test reads back: every picture a probe saved, by name,
 * as the bytes of its file. */
#ifndef AV_SCRIPTED_IMAGE_SINK_HPP
#define AV_SCRIPTED_IMAGE_SINK_HPP

#include "guest/suite/image_sink.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ScriptedImageSink final : public ImageSink {
public:
    static constexpr std::size_t max_images = 4u;

    ScriptedImageSink();

    void save(const char *name, const BmpImage &image) override;

    std::size_t          saved() const;
    /* The picture of that name, null when none was saved or it did not encode. */
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

}  // namespace av

#endif  // AV_SCRIPTED_IMAGE_SINK_HPP
