#include "harness/context/guest_blob_maker.hpp"

#include "device/control/control_request.hpp"
#include "device/memory/blob_table.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

namespace av {

namespace {
constexpr std::size_t entries_at = 24u;

}

GuestBlobMaker::GuestBlobMaker(MockHost &host, BlobTable &blobs) : host_(host), blobs_(blobs), next_(base), last_(base)
{
    host_.add_guest_ram(base, bytes);
}

Status GuestBlobMaker::create(std::uint32_t id, std::uint64_t size)
{
    const std::uint64_t pages = (size + page - 1u) / page * page;
    if (size == 0u || next_ + pages > base + bytes) return Status::no_space;
    GpuRequestBuilder entries(0u);
    entries.u64(next_).u32(static_cast<std::uint32_t>(size)).u32(0u);
    MockRequest    mock(entries.bytes(), entries.size());
    ControlRequest request(host_, &mock);
    const Status   created = blobs_.create_guest(id, size, request, entries_at, 1u);
    if (created != Status::ok) return created;
    last_ = next_;
    next_ += pages;
    return Status::ok;
}

std::uint8_t *GuestBlobMaker::last_pages() const
{
    return host_.guest_ram(last_);
}

}
