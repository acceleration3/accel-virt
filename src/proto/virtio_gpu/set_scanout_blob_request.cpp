#include "proto/virtio_gpu/set_scanout_blob_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

SetScanoutBlobRequest::SetScanoutBlobRequest()
    : scanout_id_(0u), resource_id_(0u), width_(0u), height_(0u), format_(0u), stride_(0u), offset_(0u),
      decoded_(false)
{
    AV_SCOPE("SetScanoutBlobRequest::SetScanoutBlobRequest");
}

Status SetScanoutBlobRequest::decode(WireReader &reader)
{
    AV_SCOPE("SetScanoutBlobRequest::decode");
    reader.skip(header_size);
    if (rect_.decode(reader) != Status::ok) {
        forget();
        return Status::malformed;
    }
    scanout_id_ = reader.read_u32();
    resource_id_ = reader.read_u32();
    width_ = reader.read_u32();
    height_ = reader.read_u32();
    format_ = reader.read_u32();
    const uint32_t padding = reader.read_u32();
    stride_ = reader.read_u32();
    uint32_t other_planes = 0u;
    for (uint32_t plane = 1u; plane < planes; plane++) other_planes |= reader.read_u32();
    offset_ = reader.read_u32();
    for (uint32_t plane = 1u; plane < planes; plane++) other_planes |= reader.read_u32();
    if (reader.status() != Status::ok || padding != 0u || other_planes != 0u) {
        forget();
        return Status::malformed;
    }
    decoded_ = true;
    return Status::ok;
}

void SetScanoutBlobRequest::forget()
{
    AV_SCOPE("SetScanoutBlobRequest::forget");
    rect_ = GpuRect();
    scanout_id_ = 0u;
    resource_id_ = 0u;
    width_ = 0u;
    height_ = 0u;
    format_ = 0u;
    stride_ = 0u;
    offset_ = 0u;
    decoded_ = false;
}

const GpuRect &SetScanoutBlobRequest::rect() const
{
    AV_SCOPE("SetScanoutBlobRequest::rect");
    return rect_;
}

uint32_t SetScanoutBlobRequest::scanout_id() const
{
    AV_SCOPE("SetScanoutBlobRequest::scanout_id");
    return scanout_id_;
}

uint32_t SetScanoutBlobRequest::resource_id() const
{
    AV_SCOPE("SetScanoutBlobRequest::resource_id");
    return resource_id_;
}

uint32_t SetScanoutBlobRequest::width() const
{
    AV_SCOPE("SetScanoutBlobRequest::width");
    return width_;
}

uint32_t SetScanoutBlobRequest::height() const
{
    AV_SCOPE("SetScanoutBlobRequest::height");
    return height_;
}

uint32_t SetScanoutBlobRequest::format() const
{
    AV_SCOPE("SetScanoutBlobRequest::format");
    return format_;
}

uint32_t SetScanoutBlobRequest::stride() const
{
    AV_SCOPE("SetScanoutBlobRequest::stride");
    return stride_;
}

uint32_t SetScanoutBlobRequest::offset() const
{
    AV_SCOPE("SetScanoutBlobRequest::offset");
    return offset_;
}

bool SetScanoutBlobRequest::decoded() const
{
    AV_SCOPE("SetScanoutBlobRequest::decoded");
    return decoded_;
}

}  // namespace av
