#include "guest/transport/host_window_chunk.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/resources/host_window_record.hpp"

#include <stddef.h>

namespace av {

namespace {

constexpr uint64_t page_bytes = 4096u;

}  // namespace

HostWindowChunk::HostWindowChunk(GuestConnection &connection, KmtTransport &transport)
    : connection_(connection), transport_(transport), resource_id_(0u), bytes_(0u), view_(nullptr)
{
    AV_SCOPE("HostWindowChunk::HostWindowChunk");
}

HostWindowChunk::~HostWindowChunk()
{
    AV_SCOPE("HostWindowChunk::~HostWindowChunk");
    static_cast<void>(release());
}

Status HostWindowChunk::send(uint8_t *record, uint32_t size)
{
    AV_SCOPE("HostWindowChunk::send");
    return connection_.kmt().escape(connection_.adapter(), connection_.device(), transport_.context(), record,
                                    size);
}

Status HostWindowChunk::open(uint64_t bytes)
{
    AV_SCOPE("HostWindowChunk::open");
    if (resource_id_ != 0u) return Status::invalid_state;
    if (bytes == 0u) return Status::invalid_parameter;
    HostWindowRecord record(HostWindowRecord::Operation::open, bytes, 0u);
    uint8_t          encoded[HostWindowRecord::encoded_size] = {};
    static_cast<void>(record.encode(encoded, sizeof encoded));   /* the buffer is the encoded size */
    const Status escaped = send(encoded, sizeof encoded);
    if (escaped != Status::ok) return escaped;
    const Status answered = record.accept_answer(encoded, sizeof encoded);
    if (answered != Status::ok) return answered;
    if (record.bytes() % page_bytes != 0u || record.address() % page_bytes != 0u) return Status::malformed;
    resource_id_ = record.resource_id();
    bytes_ = record.bytes();
    view_ = reinterpret_cast<unsigned char *>(static_cast<uintptr_t>(record.address()));
    return Status::ok;
}

Status HostWindowChunk::release()
{
    AV_SCOPE("HostWindowChunk::release");
    if (resource_id_ == 0u) return Status::ok;
    HostWindowRecord close(HostWindowRecord::Operation::close, 0u, resource_id_);
    resource_id_ = 0u;
    bytes_ = 0u;
    view_ = nullptr;
    /* The kernel refuses a close on a context the host faulted, and closes
     * every chunk the context holds as it is destroyed. */
    if (transport_.faulted()) return Status::ok;
    uint8_t encoded[HostWindowRecord::encoded_size] = {};
    static_cast<void>(close.encode(encoded, sizeof encoded));   /* the buffer is the encoded size */
    const Status escaped = send(encoded, sizeof encoded);
    if (escaped != Status::ok) return escaped;
    return close.accept_answer(encoded, sizeof encoded);
}

uint32_t HostWindowChunk::resource_id() const
{
    AV_SCOPE("HostWindowChunk::resource_id");
    return resource_id_;
}

uint64_t HostWindowChunk::bytes() const
{
    AV_SCOPE("HostWindowChunk::bytes");
    return bytes_;
}

volatile unsigned char *HostWindowChunk::view() const
{
    AV_SCOPE("HostWindowChunk::view");
    return view_;
}

bool HostWindowChunk::invariants_hold() const
{
    AV_SCOPE("HostWindowChunk::invariants_hold");
    if (resource_id_ == 0u) return bytes_ == 0u && view_ == nullptr;
    return bytes_ != 0u && bytes_ % page_bytes == 0u && view_ != nullptr;
}

}  // namespace av
