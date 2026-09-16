#include "guest/transport/kmt_blob_arena.hpp"

#include "core/support/scope_exit.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/client_blob_arena.hpp"

namespace av {

namespace {

constexpr uint64_t page_bytes = 4096u;
/* The executor takes the region whatever memory type it is named as. */
constexpr uint32_t any_memory_type = 0u;

}  // namespace

KmtBlobArena::KmtBlobArena(GuestConnection &connection, KmtTransport &transport)
    : transport_(transport), chunk_(connection, transport), import_(transport), bytes_(0u) {}

KmtBlobArena::~KmtBlobArena() { static_cast<void>(close()); }

Status KmtBlobArena::open(uint64_t bytes)
{
    if (bytes_ != 0u) return Status::invalid_state;
    if (bytes > max_bytes) return Status::invalid_parameter;
    const uint64_t pages = (bytes + page_bytes - 1u) / page_bytes;
    const uint64_t size = pages * page_bytes < default_bytes ? default_bytes : pages * page_bytes;
    ScopeExit      undo([&] {
        static_cast<void>(import_.release());
        static_cast<void>(chunk_.release());
    });
    Status status = chunk_.allocate(size);
    if (status == Status::ok) status = chunk_.lock();
    if (status == Status::ok) status = chunk_.bind();
    if (status == Status::ok) status = import_.import(chunk_.resource_id(), 0u, size, any_memory_type);
    if (status != Status::ok) return status;
    undo.dismiss();
    bytes_ = size;
    transport_.blobs().lend(const_cast<uint8_t *>(chunk_.view()), static_cast<size_t>(size));
    return Status::ok;
}

Status KmtBlobArena::close()
{
    if (bytes_ == 0u) return Status::ok;
    transport_.blobs().lend(nullptr, 0u);
    bytes_ = 0u;
    const Status released = import_.release();
    const Status unbound = chunk_.release();
    return released != Status::ok ? released : unbound;
}

bool     KmtBlobArena::lending() const { return bytes_ != 0u; }
uint64_t KmtBlobArena::bytes() const { return bytes_; }

bool KmtBlobArena::invariants_hold() const
{
    if (bytes_ == 0u) return !transport_.blobs().ready();
    return bytes_ % page_bytes == 0u && bytes_ >= default_bytes && bytes_ <= max_bytes &&
           transport_.blobs().ready();
}

}  // namespace av
