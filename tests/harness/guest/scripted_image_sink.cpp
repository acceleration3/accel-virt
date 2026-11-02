#include "harness/guest/scripted_image_sink.hpp"

#include <cstring>

namespace av {

ScriptedImageSink::ScriptedImageSink() : kept_{}, saved_(0u) {}

void ScriptedImageSink::save(const char *name, const BmpImage &image)
{
    if (saved_ == max_images) return;
    Kept &kept = kept_[saved_++];
    std::strncpy(kept.name, name, sizeof kept.name - 1u);
    kept.size = image.encode(kept.bytes, sizeof kept.bytes) == Status::ok ? image.encoded_size() : 0u;
}

std::size_t ScriptedImageSink::saved() const { return saved_; }

const ScriptedImageSink::Kept *ScriptedImageSink::find(const char *name) const
{
    for (std::size_t index = 0u; index < saved_; index++)
        if (std::strcmp(kept_[index].name, name) == 0) return &kept_[index];
    return nullptr;
}

const std::uint8_t *ScriptedImageSink::bytes(const char *name) const
{
    const Kept *const kept = find(name);
    return kept != nullptr && kept->size != 0u ? kept->bytes : nullptr;
}

std::size_t ScriptedImageSink::size(const char *name) const
{
    const Kept *const kept = find(name);
    return kept != nullptr ? kept->size : 0u;
}

}
