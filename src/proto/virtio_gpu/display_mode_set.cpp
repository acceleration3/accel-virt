#include "proto/virtio_gpu/display_mode_set.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

DisplayModeSet::DisplayModeSet() : widths_(), heights_(), count_(0u), preferred_(0u)
{
    AV_SCOPE("DisplayModeSet::DisplayModeSet");
}

Status DisplayModeSet::add(uint32_t width, uint32_t height)
{
    AV_SCOPE("DisplayModeSet::add");
    if (width == 0u || height == 0u || width > max_dimension || height > max_dimension)
        return Status::invalid_parameter;
    for (uint32_t index = 0u; index < count_; index++)
        if (widths_[index] == width && heights_[index] == height) return Status::ok;
    if (count_ == max_modes) return Status::no_space;
    widths_[count_] = width;
    heights_[count_] = height;
    count_++;
    return Status::ok;
}

Status DisplayModeSet::prefer(uint32_t index)
{
    AV_SCOPE("DisplayModeSet::prefer");
    if (index >= count_) return Status::invalid_parameter;
    preferred_ = index;
    return Status::ok;
}

void DisplayModeSet::clear()
{
    AV_SCOPE("DisplayModeSet::clear");
    count_ = 0u;
    preferred_ = 0u;
}

uint32_t DisplayModeSet::count() const
{
    AV_SCOPE("DisplayModeSet::count");
    return count_;
}

uint32_t DisplayModeSet::width(uint32_t index) const
{
    AV_SCOPE("DisplayModeSet::width");
    return index < count_ ? widths_[index] : 0u;
}

uint32_t DisplayModeSet::height(uint32_t index) const
{
    AV_SCOPE("DisplayModeSet::height");
    return index < count_ ? heights_[index] : 0u;
}

uint32_t DisplayModeSet::preferred() const
{
    AV_SCOPE("DisplayModeSet::preferred");
    return preferred_;
}

bool DisplayModeSet::same_as(const DisplayModeSet &other) const
{
    AV_SCOPE("DisplayModeSet::same_as");
    if (count_ != other.count_ || preferred_ != other.preferred_) return false;
    for (uint32_t index = 0u; index < count_; index++)
        if (widths_[index] != other.widths_[index] || heights_[index] != other.heights_[index]) return false;
    return true;
}

Status DisplayModeSet::encode(WireWriter &writer) const
{
    AV_SCOPE("DisplayModeSet::encode");
    if (count_ == 0u) return Status::invalid_state;
    writer.write_u32(response_type);
    writer.write_zeros(header_size - sizeof(uint32_t));
    writer.write_u32(count_);
    writer.write_u32(preferred_);
    for (uint32_t index = 0u; index < count_; index++) {
        writer.write_u32(widths_[index]);
        writer.write_u32(heights_[index]);
    }
    return writer.status();
}

size_t DisplayModeSet::encoded_size() const
{
    AV_SCOPE("DisplayModeSet::encoded_size");
    return fixed_size + count_ * mode_size;
}

Status DisplayModeSet::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("DisplayModeSet::decode");
    if (bytes == nullptr || length < fixed_size) return Status::malformed;
    WireReader reader(bytes, length);
    if (reader.read_u32() != response_type) return Status::malformed;
    for (size_t byte = sizeof(uint32_t); byte < header_size; byte++) static_cast<void>(reader.read_u8());
    const uint32_t count = reader.read_u32();
    const uint32_t preferred = reader.read_u32();
    if (count == 0u || count > max_modes || preferred >= count) return Status::malformed;
    if (reader.remaining() < count * mode_size) return Status::malformed;
    DisplayModeSet decoded;
    for (uint32_t index = 0u; index < count; index++) {
        const uint32_t width = reader.read_u32();
        const uint32_t height = reader.read_u32();
        /* A repeat is the device's to avoid; kept, it would be one mode
         * offered twice and every index after it off by one. */
        const uint32_t before = decoded.count_;
        if (decoded.add(width, height) != Status::ok || decoded.count_ == before) return Status::malformed;
    }
    decoded.preferred_ = preferred;
    *this = decoded;
    return Status::ok;
}

bool DisplayModeSet::invariants_hold() const
{
    AV_SCOPE("DisplayModeSet::invariants_hold");
    if (count_ > max_modes) return false;
    if (count_ == 0u ? preferred_ != 0u : preferred_ >= count_) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        if (widths_[index] == 0u || heights_[index] == 0u) return false;
        if (widths_[index] > max_dimension || heights_[index] > max_dimension) return false;
        for (uint32_t other = 0u; other < index; other++)
            if (widths_[other] == widths_[index] && heights_[other] == heights_[index]) return false;
    }
    return true;
}

}  // namespace av
