#include "proto/virtio_gpu/display_mode_set.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

DisplayModeSet::DisplayModeSet() : widths_(), heights_(), count_(0u), preferred_(0u), refresh_millihz_(default_refresh_millihz)
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

Status DisplayModeSet::set_refresh(uint32_t millihz)
{
    AV_SCOPE("DisplayModeSet::set_refresh");
    if (millihz < least_refresh_millihz || millihz > most_refresh_millihz) return Status::invalid_parameter;
    refresh_millihz_ = millihz;
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

uint32_t DisplayModeSet::refresh_millihz() const
{
    AV_SCOPE("DisplayModeSet::refresh_millihz");
    return refresh_millihz_;
}

bool DisplayModeSet::same_as(const DisplayModeSet &other) const
{
    AV_SCOPE("DisplayModeSet::same_as");
    if (count_ != other.count_ || preferred_ != other.preferred_ || refresh_millihz_ != other.refresh_millihz_)
        return false;
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
    writer.write_u32(refresh_millihz_);
    return writer.status();
}

size_t DisplayModeSet::encoded_size() const
{
    AV_SCOPE("DisplayModeSet::encoded_size");
    return fixed_size + count_ * mode_size + refresh_size;
}

Status DisplayModeSet::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("DisplayModeSet::decode");
    if (bytes == nullptr || length < fixed_size) return Status::malformed;
    WireReader reader(bytes, length);
    if (reader.read_u32() != response_type) return Status::malformed;
    reader.skip(header_size - sizeof(uint32_t));
    const uint32_t count = reader.read_u32();
    const uint32_t preferred = reader.read_u32();
    if (count == 0u || count > max_modes || preferred >= count) return Status::malformed;
    if (reader.remaining() < count * mode_size) return Status::malformed;
    DisplayModeSet decoded;
    for (uint32_t index = 0u; index < count; index++) {
        const uint32_t width = reader.read_u32();
        const uint32_t height = reader.read_u32();
        const uint32_t before = decoded.count_;
        if (decoded.add(width, height) != Status::ok || decoded.count_ == before) return Status::malformed;
    }
    decoded.preferred_ = preferred;
    if (reader.remaining() >= refresh_size) {
        const uint32_t refresh = reader.read_u32();
        if (refresh != 0u && decoded.set_refresh(refresh) != Status::ok) return Status::malformed;
    }
    *this = decoded;
    return Status::ok;
}

bool DisplayModeSet::invariants_hold() const
{
    AV_SCOPE("DisplayModeSet::invariants_hold");
    if (count_ > max_modes) return false;
    if (count_ == 0u ? preferred_ != 0u : preferred_ >= count_) return false;
    if (refresh_millihz_ < least_refresh_millihz || refresh_millihz_ > most_refresh_millihz) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        if (widths_[index] == 0u || heights_[index] == 0u) return false;
        if (widths_[index] > max_dimension || heights_[index] > max_dimension) return false;
        for (uint32_t other = 0u; other < index; other++)
            if (widths_[other] == widths_[index] && heights_[other] == heights_[index]) return false;
    }
    return true;
}

}
