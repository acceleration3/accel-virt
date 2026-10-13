#include "proto/virtio_gpu/scanout_plane_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

/* A rectangle with room in it, left of its right and above its bottom. */
bool spans(const int32_t *rect)
{
    AV_SCOPE("spans");
    return rect[0] < rect[2] && rect[1] < rect[3];
}

}  // namespace

ScanoutPlaneRequest::ScanoutPlaneRequest()
    : head_(0u), layer_(0u), flags_(0u), key_(0u), source_{}, destination_{}, head_width_(0u), head_height_(0u)
{
    AV_SCOPE("ScanoutPlaneRequest::ScanoutPlaneRequest");
}

ScanoutPlaneRequest::ScanoutPlaneRequest(uint32_t head, uint32_t layer, uint32_t flags, uint64_t key,
                                         const int32_t *source, const int32_t *destination, uint32_t head_width,
                                         uint32_t head_height)
    : head_(head), layer_(layer), flags_(flags), key_(key), source_{}, destination_{}, head_width_(head_width),
      head_height_(head_height)
{
    AV_SCOPE("ScanoutPlaneRequest::ScanoutPlaneRequest");
    for (uint32_t edge = 0u; edge < 4u; edge++) {
        source_[edge] = source != nullptr ? source[edge] : 0;
        destination_[edge] = destination != nullptr ? destination[edge] : 0;
    }
}

Status ScanoutPlaneRequest::encode(WireWriter &writer) const
{
    AV_SCOPE("ScanoutPlaneRequest::encode");
    writer.write_u32(type);
    writer.write_zeros(header_size - sizeof(uint32_t));
    writer.write_u32(head_);
    writer.write_u32(layer_);
    writer.write_u32(flags_);
    writer.write_u32(0u);
    writer.write_u64(key_);
    for (const int32_t edge : source_) writer.write_u32(static_cast<uint32_t>(edge));
    for (const int32_t edge : destination_) writer.write_u32(static_cast<uint32_t>(edge));
    writer.write_u32(head_width_);
    writer.write_u32(head_height_);
    return writer.status();
}

Status ScanoutPlaneRequest::decode(WireReader &reader)
{
    AV_SCOPE("ScanoutPlaneRequest::decode");
    if (reader.remaining() < encoded_size) return Status::malformed;
    reader.skip(header_size);
    ScanoutPlaneRequest read;
    read.head_ = reader.read_u32();
    read.layer_ = reader.read_u32();
    read.flags_ = reader.read_u32();
    static_cast<void>(reader.read_u32());
    read.key_ = reader.read_u64();
    for (int32_t &edge : read.source_) edge = static_cast<int32_t>(reader.read_u32());
    for (int32_t &edge : read.destination_) edge = static_cast<int32_t>(reader.read_u32());
    read.head_width_ = reader.read_u32();
    read.head_height_ = reader.read_u32();
    if (read.layer_ >= layers || read.head_width_ == 0u || read.head_height_ == 0u ||
        read.head_width_ > most_extent || read.head_height_ > most_extent)
        return Status::malformed;
    if (read.shown() && (read.key_ == 0u || !spans(read.source_) || !spans(read.destination_) ||
                         read.source_[0] < 0 || read.source_[1] < 0))
        return Status::malformed;
    *this = read;
    return Status::ok;
}

uint32_t ScanoutPlaneRequest::head() const
{
    AV_SCOPE("ScanoutPlaneRequest::head");
    return head_;
}

uint32_t ScanoutPlaneRequest::layer() const
{
    AV_SCOPE("ScanoutPlaneRequest::layer");
    return layer_;
}

bool ScanoutPlaneRequest::shown() const
{
    AV_SCOPE("ScanoutPlaneRequest::shown");
    return (flags_ & flag_shown) != 0u;
}

bool ScanoutPlaneRequest::filtered() const
{
    AV_SCOPE("ScanoutPlaneRequest::filtered");
    return (flags_ & flag_filtered) != 0u;
}

bool ScanoutPlaneRequest::ycbcr() const
{
    AV_SCOPE("ScanoutPlaneRequest::ycbcr");
    return (flags_ & flag_ycbcr) != 0u;
}

bool ScanoutPlaneRequest::bt709() const
{
    AV_SCOPE("ScanoutPlaneRequest::bt709");
    return (flags_ & flag_bt709) != 0u;
}

bool ScanoutPlaneRequest::full_range() const
{
    AV_SCOPE("ScanoutPlaneRequest::full_range");
    return (flags_ & flag_full_range) != 0u;
}

uint64_t ScanoutPlaneRequest::key() const
{
    AV_SCOPE("ScanoutPlaneRequest::key");
    return key_;
}

const int32_t *ScanoutPlaneRequest::source() const
{
    AV_SCOPE("ScanoutPlaneRequest::source");
    return source_;
}

const int32_t *ScanoutPlaneRequest::destination() const
{
    AV_SCOPE("ScanoutPlaneRequest::destination");
    return destination_;
}

uint32_t ScanoutPlaneRequest::head_width() const
{
    AV_SCOPE("ScanoutPlaneRequest::head_width");
    return head_width_;
}

uint32_t ScanoutPlaneRequest::head_height() const
{
    AV_SCOPE("ScanoutPlaneRequest::head_height");
    return head_height_;
}

}  // namespace av
