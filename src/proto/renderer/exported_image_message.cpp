#include "proto/renderer/exported_image_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ExportedImageMessage::ExportedImageMessage()
    : found_(0u), fresh_(0u), slot_(0u), width_(0u), height_(0u), stride_(0u), offset_(0u), fourcc_(0u), modifier_(0u)
{
    AV_SCOPE("ExportedImageMessage::ExportedImageMessage");
}

ExportedImageMessage::ExportedImageMessage(bool fresh, uint32_t slot, uint32_t width, uint32_t height,
                                           uint32_t stride, uint32_t offset, uint32_t fourcc, uint64_t modifier)
    : found_(1u), fresh_(fresh ? 1u : 0u), slot_(slot), width_(width), height_(height), stride_(stride),
      offset_(offset), fourcc_(fourcc), modifier_(modifier)
{
    AV_SCOPE("ExportedImageMessage::ExportedImageMessage");
}

bool ExportedImageMessage::found() const
{
    AV_SCOPE("ExportedImageMessage::found");
    return found_ != 0u;
}

bool ExportedImageMessage::fresh() const
{
    AV_SCOPE("ExportedImageMessage::fresh");
    return fresh_ != 0u;
}

uint32_t ExportedImageMessage::slot() const
{
    AV_SCOPE("ExportedImageMessage::slot");
    return slot_;
}

uint32_t ExportedImageMessage::width() const
{
    AV_SCOPE("ExportedImageMessage::width");
    return width_;
}

uint32_t ExportedImageMessage::height() const
{
    AV_SCOPE("ExportedImageMessage::height");
    return height_;
}

uint32_t ExportedImageMessage::stride() const
{
    AV_SCOPE("ExportedImageMessage::stride");
    return stride_;
}

uint32_t ExportedImageMessage::offset() const
{
    AV_SCOPE("ExportedImageMessage::offset");
    return offset_;
}

uint32_t ExportedImageMessage::fourcc() const
{
    AV_SCOPE("ExportedImageMessage::fourcc");
    return fourcc_;
}

uint64_t ExportedImageMessage::modifier() const
{
    AV_SCOPE("ExportedImageMessage::modifier");
    return modifier_;
}

uint32_t ExportedImageMessage::descriptors() const
{
    AV_SCOPE("ExportedImageMessage::descriptors");
    return found_ != 0u && fresh_ != 0u ? 1u : 0u;
}

bool ExportedImageMessage::fields_valid() const
{
    AV_SCOPE("ExportedImageMessage::fields_valid");
    if (found_ > 1u || fresh_ > 1u) return false;
    if (found_ == 0u)
        return fresh_ == 0u && slot_ == 0u && width_ == 0u && height_ == 0u && stride_ == 0u && offset_ == 0u &&
               fourcc_ == 0u && modifier_ == 0u;
    return slot_ < slots && width_ != 0u && height_ != 0u && stride_ != 0u;
}

Status ExportedImageMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ExportedImageMessage::encode");
    if (bytes == nullptr || capacity < encoded_size) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(found_);
    writer.write_u32(fresh_);
    writer.write_u32(slot_);
    writer.write_u32(width_);
    writer.write_u32(height_);
    writer.write_u32(stride_);
    writer.write_u32(offset_);
    writer.write_u32(fourcc_);
    writer.write_u64(modifier_);
    return writer.status();
}

Status ExportedImageMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("ExportedImageMessage::decode");
    if (bytes == nullptr || length != encoded_size) return Status::malformed;
    WireReader           reader(bytes, length);
    ExportedImageMessage candidate;
    candidate.found_ = reader.read_u32();
    candidate.fresh_ = reader.read_u32();
    candidate.slot_ = reader.read_u32();
    candidate.width_ = reader.read_u32();
    candidate.height_ = reader.read_u32();
    candidate.stride_ = reader.read_u32();
    candidate.offset_ = reader.read_u32();
    candidate.fourcc_ = reader.read_u32();
    candidate.modifier_ = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (!candidate.fields_valid() || descriptor_count != candidate.descriptors()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
