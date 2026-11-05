#include "guest/transport/transport_ring.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

TransportRing::TransportRing() : ring_(nullptr), order_(0u), tail_(0u)
{
    AV_SCOPE("TransportRing::TransportRing");
}

Status TransportRing::initialize(uint8_t *ring, uint8_t order)
{
    AV_SCOPE("TransportRing::initialize");
    if (ring_ != nullptr) return Status::invalid_state;
    if (ring == nullptr || order < RingHeader::lowest_order || order > RingHeader::highest_order)
        return Status::invalid_parameter;
    static_cast<void>(RingHeader(order, 0u, 0u).encode(ring, header_size));
    ring_ = ring;
    order_ = order;
    tail_ = 0u;
    return Status::ok;
}

Status TransportRing::write_batch(const uint8_t *payload, uint32_t length, const uint8_t *trailer,
                                  uint32_t trailer_length, uint64_t fence_id, uint64_t published_at,
                                  uint64_t &begin, uint64_t &end)
{
    AV_SCOPE("TransportRing::write_batch");
    if (ring_ == nullptr) return Status::invalid_state;
    if (fence_id == 0u || payload == nullptr || (trailer == nullptr && trailer_length != 0u))
        return Status::invalid_parameter;
    const uint64_t total = uint64_t{length} + trailer_length;
    if (total > 0xFFFFFFFFu) return Status::invalid_parameter;
    const BatchHeader header(static_cast<uint32_t>(total), fence_id, published_at);
    if (!header.fits(capacity())) return Status::invalid_parameter;
    const uint64_t span = header.span();
    if (span > free_space()) return Status::no_space;
    uint8_t framing[BatchHeader::encoded_size] = {};
    static_cast<void>(header.encode(framing, sizeof framing));
    begin = tail_;
    write_wrapped(tail_, framing, BatchHeader::encoded_size);
    write_wrapped(tail_ + BatchHeader::encoded_size, payload, length);
    if (trailer_length != 0u) write_wrapped(tail_ + BatchHeader::encoded_size + length, trailer, trailer_length);
    tail_ += span;
    end = tail_;
    return Status::ok;
}

void TransportRing::write_wrapped(uint64_t counter, const uint8_t *source, uint32_t count)
{
    AV_SCOPE("TransportRing::write_wrapped");
    const uint64_t mask = capacity() - 1u;
    uint8_t *const data = ring_ + header_size;
    const size_t at = static_cast<size_t>(counter & mask);
    const size_t first = static_cast<size_t>(capacity() - at < count ? capacity() - at : count);
    __builtin_memcpy(data + at, source, first);
    __builtin_memcpy(data, source + first, count - first);
}

uint64_t TransportRing::read_head() const
{
    AV_SCOPE("TransportRing::read_head");
    if (ring_ == nullptr) return 0u;
    return __atomic_load_n(reinterpret_cast<const uint64_t *>(ring_ + head_offset), __ATOMIC_ACQUIRE);
}

Status TransportRing::publish_tail()
{
    AV_SCOPE("TransportRing::publish_tail");
    if (ring_ == nullptr) return Status::invalid_state;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    __atomic_store_n(reinterpret_cast<uint64_t *>(ring_ + tail_offset), tail_, __ATOMIC_RELEASE);
    return Status::ok;
}

bool TransportRing::consumer_awake() const
{
    AV_SCOPE("TransportRing::consumer_awake");
    if (ring_ == nullptr) return false;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return __atomic_load_n(ring_ + awake_offset, __ATOMIC_ACQUIRE) != 0u;
}

uint64_t TransportRing::tail() const
{
    AV_SCOPE("TransportRing::tail");
    return tail_;
}
uint64_t TransportRing::head() const
{
    AV_SCOPE("TransportRing::head");
    return read_head();
}
uint64_t TransportRing::capacity() const
{
    AV_SCOPE("TransportRing::capacity");
    return ring_ == nullptr ? 0u : uint64_t{1} << order_;
}

uint64_t TransportRing::free_space() const
{
    AV_SCOPE("TransportRing::free_space");
    const uint64_t used = tail_ - read_head();
    return capacity() - used;
}

bool TransportRing::invariants_hold() const
{
    AV_SCOPE("TransportRing::invariants_hold");
    if (ring_ == nullptr) return tail_ == 0u && order_ == 0u;
    const uint64_t head = read_head();
    return tail_ >= head && tail_ - head <= capacity();
}

}
