/* A context's ring as a hostile guest writes it: the consumer serves after each
 * change to the ring header or data the input makes. Whatever the bytes, the
 * consumer stays consistent, its head only moves forward and never past what
 * was published, a stopped context stays stopped, and nothing is read or
 * written outside the ring's blob (ASan).
 *
 * Input: u16 initial tail, then operations of [u8 kind][u16 position][u8 count]
 * followed by `count` bytes - kind 0 writes the data area at a ring counter,
 * 1 writes the tail, 2 writes raw header bytes. */
#include "core/support/status.hpp"
#include "device/memory/blob_window.hpp"
#include "device/ring/ring_consumer.hpp"
#include "device/ring/ring_window.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t capacity = 4096u;

class Ring {
public:
    Ring() : bytes_{}, pointer_(bytes_), length_(sizeof bytes_) {}
    av::RingWindow window() const { return av::RingWindow(av::BlobWindow(&pointer_, &length_, 1u, length_), 12u); }
    std::uint8_t  *bytes() { return bytes_; }

private:
    std::uint8_t  bytes_[32u + capacity];
    void         *pointer_;
    std::uint64_t length_;
};

void serve(av::MockHost &host, av::RingConsumer &consumer)
{
    consumer.service();
    if (!host.claim(av::HostViolation::worker_required)) __builtin_trap();
    if (!consumer.invariants_hold()) __builtin_trap();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader reader(data, size);
    Ring           ring;
    const std::uint64_t first_tail = reader.read_u16() % (capacity + 1u);
    static_cast<void>(av::RingHeader(12u, first_tail, 0u).encode(ring.bytes(), 32u));
    av::MockHost     host;
    av::RingConsumer consumer(host);
    if (consumer.prepare(ring.window()) != av::Status::ok) __builtin_trap();
    consumer.attach();
    if (!host.claim(av::HostViolation::worker_required)) __builtin_trap();
    serve(host, consumer);
    while (reader.remaining() >= 4u) {
        const std::uint8_t  kind = reader.read_u8();
        const std::uint16_t position = reader.read_u16();
        const std::uint8_t  count = reader.read_u8();
        for (std::uint8_t i = 0u; i < count && reader.remaining() != 0u; i++) {
            const std::uint8_t byte = reader.read_u8();
            if (kind % 3u == 0u) ring.bytes()[32u + ((position + i) & (capacity - 1u))] = byte;
            if (kind % 3u == 1u) ring.bytes()[8u + (i % 8u)] = byte;
            if (kind % 3u == 2u) ring.bytes()[(position + i) % 32u] = byte;
        }
        const std::uint64_t head = consumer.head();
        const bool          stopped = consumer.stopped();
        serve(host, consumer);
        if (consumer.head() < head || (stopped && (!consumer.stopped() || consumer.head() != head))) __builtin_trap();
    }
    return 0;
}
