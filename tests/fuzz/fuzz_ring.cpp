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
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/blob_window.hpp"
#include "device/ring/ring_consumer.hpp"
#include "device/ring/ring_window.hpp"
#include "harness/executor/mock_context_execution.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/submission/fence_page.hpp"
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

/* The page the guest reads says what the consumer completed, whatever else happened. */
void serve(av::MockHost &host, av::RingConsumer &consumer, const std::uint8_t *page_bytes)
{
    consumer.service();
    while (host.claim(av::HostViolation::worker_required)) {}
    if (!consumer.invariants_hold()) __builtin_trap();
    av::FencePage page;
    if (page.decode(page_bytes, av::FencePage::encoded_size) != av::Status::ok) __builtin_trap();
    if (page.completed_fence() != consumer.last_fence() || page.progress() != consumer.batches()) __builtin_trap();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader reader(data, size);
    Ring           ring;
    const std::uint64_t first_tail = reader.read_u16() % (capacity + 1u);
    static_cast<void>(av::RingHeader(12u, first_tail, 0u).encode(ring.bytes(), 32u));
    std::uint8_t        page[av::FencePage::encoded_size] = {};
    void               *page_pointer = page;
    const std::uint64_t page_length = sizeof page;
    av::MockHost        host;
    av::FenceInterrupts interrupts(host);
    av::FenceReporter   fences(host, interrupts);
    av::MockContextExecution execution(host, 12u);
    av::RingConsumer    consumer(host, fences, execution.batches());
    if (fences.prepare(av::FenceWindow(av::BlobWindow(&page_pointer, &page_length, 1u, page_length)), 0u) !=
        av::Status::ok)
        __builtin_trap();
    if (consumer.prepare(ring.window()) != av::Status::ok) __builtin_trap();
    consumer.attach();
    while (host.claim(av::HostViolation::worker_required)) {}
    serve(host, consumer, page);
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
        serve(host, consumer, page);
        if (consumer.head() < head || (stopped && (!consumer.stopped() || consumer.head() != head))) __builtin_trap();
    }
    consumer.detach();
    while (host.claim(av::HostViolation::worker_required)) {}
    return 0;
}
