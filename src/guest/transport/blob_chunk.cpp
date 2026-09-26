#include "guest/transport/blob_chunk.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint64_t page_bytes = 4096u;

}  // namespace

BlobChunk::BlobChunk(GuestConnection &connection, KmtTransport &transport)
    : window_(connection, transport), guest_ram_(connection, transport)
{
    AV_SCOPE("BlobChunk::BlobChunk");
}

Status BlobChunk::open(uint64_t bytes)
{
    AV_SCOPE("BlobChunk::open");
    if (resource_id() != 0u) return Status::invalid_state;
    const Status windowed = window_.open(bytes);
    if (windowed == Status::ok || windowed == Status::malformed) return windowed;
    return open_guest_ram(bytes);
}

Status BlobChunk::open_guest_ram(uint64_t bytes)
{
    AV_SCOPE("BlobChunk::open_guest_ram");
    Status status = guest_ram_.allocate(bytes);
    if (status == Status::ok) status = guest_ram_.lock();
    if (status == Status::ok) status = guest_ram_.bind();
    if (status == Status::ok) return Status::ok;
    static_cast<void>(guest_ram_.release());   /* the refusal above is the answer */
    return status;
}

Status BlobChunk::release()
{
    AV_SCOPE("BlobChunk::release");
    const Status window = window_.release();
    const Status guest_ram = guest_ram_.release();
    return window != Status::ok ? window : guest_ram;
}

uint32_t BlobChunk::resource_id() const
{
    AV_SCOPE("BlobChunk::resource_id");
    return window_.resource_id() != 0u ? window_.resource_id() : guest_ram_.resource_id();
}

uint64_t BlobChunk::bytes() const
{
    AV_SCOPE("BlobChunk::bytes");
    return window_.resource_id() != 0u ? window_.bytes() : guest_ram_.pages() * page_bytes;
}

volatile unsigned char *BlobChunk::view() const
{
    AV_SCOPE("BlobChunk::view");
    if (window_.resource_id() != 0u) return window_.view();
    return guest_ram_.resource_id() != 0u ? guest_ram_.view() : nullptr;
}

bool BlobChunk::invariants_hold() const
{
    AV_SCOPE("BlobChunk::invariants_hold");
    if (!window_.invariants_hold() || !guest_ram_.invariants_hold()) return false;
    return window_.resource_id() == 0u || guest_ram_.view() == nullptr;
}

}  // namespace av
