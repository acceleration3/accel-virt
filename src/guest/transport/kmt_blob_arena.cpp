#include "guest/transport/kmt_blob_arena.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/client/client_blob_arena.hpp"

namespace av {

namespace {

constexpr uint64_t page_bytes = 4096u;

}  // namespace

KmtBlobArena::KmtBlobArena(GuestConnection &connection, KmtTransport &transport)
    : transport_(transport), memory_(connection, transport), bytes_(0u)
    {
        AV_SCOPE("KmtBlobArena::KmtBlobArena");
    }

KmtBlobArena::~KmtBlobArena()
{
    AV_SCOPE("KmtBlobArena::~KmtBlobArena");
    static_cast<void>(close());
}

Status KmtBlobArena::open(uint64_t bytes)
{
    AV_SCOPE("KmtBlobArena::open");
    if (bytes_ != 0u) return Status::invalid_state;
    if (bytes > max_bytes) return Status::invalid_parameter;
    const uint64_t pages = (bytes + page_bytes - 1u) / page_bytes;
    const uint64_t size = pages * page_bytes < default_bytes ? default_bytes : pages * page_bytes;
    const Status opened = memory_.open(size, HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u));
    if (opened != Status::ok) return opened;
    bytes_ = size;
    /* The host writes the word only once it publishes one: zero is what an
     * older host leaves, which the context reads as "ask instead". */
    volatile unsigned char *const word = memory_.view() + size - error_word_bytes;
    for (uint64_t at = 0u; at < error_word_bytes; at++) word[at] = 0u;
    transport_.blobs().lend(memory_.view(), static_cast<size_t>(size - error_word_bytes));
    return Status::ok;
}

Status KmtBlobArena::close()
{
    AV_SCOPE("KmtBlobArena::close");
    if (bytes_ == 0u) return Status::ok;
    transport_.blobs().lend(nullptr, 0u);
    bytes_ = 0u;
    return memory_.close();
}

Status KmtBlobArena::abandon()
{
    AV_SCOPE("KmtBlobArena::abandon");
    if (bytes_ == 0u) return Status::ok;
    transport_.blobs().lend(nullptr, 0u);
    bytes_ = 0u;
    return memory_.abandon();
}

bool     KmtBlobArena::lending() const
{
    AV_SCOPE("KmtBlobArena::lending");
    return bytes_ != 0u;
}
uint64_t KmtBlobArena::bytes() const
{
    AV_SCOPE("KmtBlobArena::bytes");
    return bytes_;
}

uint8_t *KmtBlobArena::view() const
{
    AV_SCOPE("KmtBlobArena::view");
    if (bytes_ == 0u) return nullptr;
    return memory_.view();
}

uint64_t KmtBlobArena::host_errors() const
{
    AV_SCOPE("KmtBlobArena::host_errors");
    if (bytes_ == 0u) return 0u;
    const auto *const word = reinterpret_cast<const volatile uint64_t *>(memory_.view() + bytes_ - error_word_bytes);
    return __atomic_load_n(word, __ATOMIC_ACQUIRE);
}

bool KmtBlobArena::invariants_hold() const
{
    AV_SCOPE("KmtBlobArena::invariants_hold");
    if (bytes_ == 0u) return !transport_.blobs().ready();
    return bytes_ % page_bytes == 0u && bytes_ >= default_bytes && bytes_ <= max_bytes &&
           transport_.blobs().ready();
}

}  // namespace av
