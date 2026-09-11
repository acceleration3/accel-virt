#include "harness/ring/guest_ring.hpp"

#include "proto/submission/batch_header.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/ring_header.hpp"

#include <cstring>

namespace av {

GuestRing::GuestRing(std::uint8_t *bytes, std::uint8_t order) : bytes_(bytes), order_(order), tail_(0u)
{
    static_cast<void>(RingHeader(order, 0u, 0u).encode(bytes_, RingHeader::encoded_size));
}

void GuestRing::write_batch(std::uint64_t fence, std::uint32_t length)
{
    const BatchHeader batch(length, fence);
    std::uint8_t      header[BatchHeader::encoded_size + CommandHeader::encoded_size] = {};
    static_cast<void>(batch.encode(header, BatchHeader::encoded_size));
    const std::uint64_t framed = length >= CommandHeader::encoded_size ? sizeof header : BatchHeader::encoded_size;
    if (framed == sizeof header)
        static_cast<void>(CommandHeader(filler_command, length, 0u).encode(header + BatchHeader::encoded_size,
                                                                           CommandHeader::encoded_size));
    for (std::uint64_t i = 0u; i < batch.span(); i++)
        data(tail_ + i) = i < framed ? header[i] : static_cast<std::uint8_t>(0xB0u + i);
    tail_ += batch.span();
}

void GuestRing::write_batch(std::uint64_t fence, const std::uint8_t *payload, std::uint32_t length)
{
    const BatchHeader batch(length, fence);
    std::uint8_t      header[BatchHeader::encoded_size] = {};
    static_cast<void>(batch.encode(header, sizeof header));
    for (std::uint64_t i = 0u; i < batch.span(); i++) {
        const bool in_payload = i >= sizeof header && i < sizeof header + length;
        data(tail_ + i) = i < sizeof header ? header[i] : in_payload ? payload[i - sizeof header] : 0u;
    }
    tail_ += batch.span();
}

void GuestRing::publish() { publish(tail_); }

void GuestRing::publish(std::uint64_t tail) { store(tail_offset, tail); }

void GuestRing::store(std::uint64_t offset, std::uint64_t value) { std::memcpy(bytes_ + offset, &value, sizeof value); }

std::uint64_t GuestRing::head() const
{
    std::uint64_t head = 0u;
    std::memcpy(&head, bytes_ + head_offset, sizeof head);
    return head;
}

std::uint64_t GuestRing::tail() const { return tail_; }

std::uint64_t GuestRing::capacity() const { return std::uint64_t{1} << order_; }

std::uint8_t &GuestRing::data(std::uint64_t counter)
{
    return bytes_[RingHeader::encoded_size + (counter & (capacity() - 1u))];
}

std::uint8_t *GuestRing::bytes() { return bytes_; }

}  // namespace av
