/* A process's chunk of the device's host window over MockKmt, which answers
 * the HostWindowRecord as the driver does (docs/host-memory.md): one escape
 * opens it - host GPU memory of the kind asked for, no allocation, no lock,
 * no paging - and one closes it, the answer is judged before anything is
 * kept, nothing falls back to guest RAM, and a faulted transport leaves the
 * close to the kernel. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/host_window_chunk.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/escape/host_window_record.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

namespace av {

struct HostWindowChunkProbe {
    static void without_view(HostWindowChunk &chunk) { chunk.view_ = nullptr; }
    static void without_bytes(HostWindowChunk &chunk) { chunk.bytes_ = 0u; }
    static void claim_bytes(HostWindowChunk &chunk, std::uint64_t bytes) { chunk.bytes_ = bytes; }
    static void without_resource(HostWindowChunk &chunk) { chunk.resource_id_ = 0u; }
    static void view_at(HostWindowChunk &chunk, unsigned char *view) { chunk.view_ = view; }
};

}  // namespace av

using av::GuestConnection;
using av::HostMemoryRequest;
using av::HostWindowChunk;
using av::HostWindowChunkProbe;
using av::HostWindowRecord;
using av::KmtTransport;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::SubApi;

namespace {

const PciIdentity av(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint64_t mib = std::uint64_t{1} << 20u;
constexpr std::uint64_t window_bytes = 256u * mib;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::uint32_t refused = 0xC0000001u;
const HostMemoryRequest upload(HostMemoryRequest::Kind::upload, 0u);
/* The connection's device and queue, the transport's two allocations, its DMA
 * mapping and its context, with the adapter. */
constexpr std::uint32_t link_objects = 7u;

class Rig {
public:
    Rig()
        : adapter_(0u), connection_(kmt_), staging_{},
          transport_(connection_, SubApi::vulkan, 12u, reply_bytes, staging_, sizeof staging_)
    {
        kmt_.add_adapter(av, 3200u);
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter_, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter_), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
    }

    MockKmt         &kmt() { return kmt_; }
    GuestConnection &connection() { return connection_; }
    KmtTransport    &transport() { return transport_; }

    void fault_the_transport()
    {
        kmt_.withhold_answer_at(1u);
        av::WireWriter request = transport_.request_writer();
        request.write_u32(1u);
        av::WireReader reply(nullptr, 0u);
        CHECK(transport_.send_sync(1u, request, reply) != av::TransportResult::ok);
        transport_.end_call();
        CHECK(transport_.faulted());
    }

    void check_only_the_link_is_left()
    {
        CHECK_EQ(kmt_.open_objects(), link_objects);
        CHECK_EQ(kmt_.window_chunks(), 0u);
        CHECK_EQ(kmt_.misuses(), 0u);
    }

private:
    MockKmt         kmt_;
    std::uint32_t   adapter_;
    GuestConnection connection_;
    std::uint8_t    staging_[1024];
    KmtTransport    transport_;
};

/* An open answer the kernel could write back, for a test to break one field of. */
void script_open_answer(MockKmt &kmt, std::uint64_t asked, std::uint32_t resource, std::uint64_t bytes,
                        std::uint64_t address)
{
    std::uint8_t answer[HostWindowRecord::encoded_size] = {};
    CHECK_STATUS(HostWindowRecord(HostWindowRecord::Operation::open, asked, 0u, upload)
                     .answered(resource, bytes, address)
                     .encode(answer, sizeof answer),
                 Status::ok);
    kmt.answer_next_escape(answer, sizeof answer);
}

}  // namespace

TEST(host_window_chunk, one_escape_opens_a_writable_chunk_and_one_closes_it)
{ Rig rig;
  rig.kmt().open_host_window(window_bytes);
  HostWindowChunk chunk(rig.connection(), rig.transport());
  CHECK(chunk.invariants_hold());
  const std::uint32_t allocations = rig.kmt().calls(MockKmt::Call::create_allocation);
  const std::uint32_t locks = rig.kmt().calls(MockKmt::Call::lock);
  const std::uint32_t residents = rig.kmt().calls(MockKmt::Call::make_resident);
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  const HostMemoryRequest vulkan(HostMemoryRequest::Kind::vulkan, 2u);
  CHECK_STATUS(chunk.open(64u * mib + 1u, vulkan), Status::ok);
  CHECK(chunk.invariants_hold());
  /* The escape is the whole of it. */
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes + 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_allocation), allocations);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::lock), locks);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::make_resident), residents);
  CHECK_EQ(rig.kmt().open_objects(), link_objects);
  /* What was sent is an open of the bytes asked, no more. */
  HostWindowRecord sent;
  CHECK_EQ(rig.kmt().last_escape_size(), HostWindowRecord::encoded_size);
  CHECK_STATUS(sent.decode(rig.kmt().last_escape(), HostWindowRecord::encoded_size), Status::ok);
  CHECK(sent.opens());
  CHECK_EQ(sent.bytes(), 64u * mib + 1u);
  CHECK_EQ(sent.memory().encoded(), vulkan.encoded());
  CHECK_EQ(rig.kmt().window_request(MockKmt::first_window_resource).encoded(), vulkan.encoded());
  /* The answer: the resource, the pages granted and where the window put it. */
  CHECK_EQ(chunk.resource_id(), MockKmt::first_window_resource);
  CHECK_EQ(chunk.bytes(), 64u * mib + 4096u);
  CHECK(chunk.view() == rig.kmt().window_base());
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  chunk.view()[chunk.bytes() - 1u] = 0x5Au;
  CHECK_EQ(rig.kmt().window_base()[64u * mib + 4095u], 0x5Au);
  CHECK_STATUS(chunk.release(), Status::ok);
  CHECK(chunk.invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes + 2u);
  HostWindowRecord closed;
  CHECK_STATUS(closed.decode(rig.kmt().last_escape(), HostWindowRecord::encoded_size), Status::ok);
  CHECK(!closed.opens());
  CHECK_EQ(closed.resource_id(), MockKmt::first_window_resource);
  CHECK_EQ(chunk.resource_id(), 0u);
  CHECK_EQ(chunk.bytes(), 0u);
  CHECK(chunk.view() == nullptr);
  /* Released twice, nothing is asked. */
  CHECK_STATUS(chunk.release(), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes + 2u);
  rig.check_only_the_link_is_left(); }

/* A second chunk lies after the first, and one opened after the first closed
 * reuses its place: the window is placed from its start. */
TEST(host_window_chunk, chunks_lie_apart_and_a_closed_place_is_reused)
{ Rig rig;
  rig.kmt().open_host_window(window_bytes);
  HostWindowChunk first(rig.connection(), rig.transport());
  HostWindowChunk second(rig.connection(), rig.transport());
  CHECK_STATUS(first.open(mib, upload), Status::ok);
  CHECK_STATUS(second.open(mib, upload), Status::ok);
  CHECK(second.view() == first.view() + mib);
  CHECK(first.resource_id() != second.resource_id());
  CHECK_STATUS(first.release(), Status::ok);
  HostWindowChunk third(rig.connection(), rig.transport());
  CHECK_STATUS(third.open(mib, upload), Status::ok);
  CHECK(third.view() == rig.kmt().window_base());
  CHECK_EQ(rig.kmt().window_chunks(), 2u);
  CHECK_STATUS(second.release(), Status::ok);
  CHECK_STATUS(third.release(), Status::ok);
  rig.check_only_the_link_is_left(); }

TEST(host_window_chunk, an_open_while_held_of_nothing_or_of_no_memory_asks_nothing)
{ Rig rig;
  rig.kmt().open_host_window(window_bytes);
  HostWindowChunk chunk(rig.connection(), rig.transport());
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(chunk.open(0u, upload), Status::invalid_parameter);
  CHECK_STATUS(chunk.open(mib, HostMemoryRequest()), Status::invalid_parameter);
  CHECK_STATUS(chunk.open(mib, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, 32u)), Status::invalid_parameter);
  CHECK_STATUS(chunk.open(mib, HostMemoryRequest(0x104u)), Status::invalid_parameter);
  CHECK_STATUS(chunk.open(mib, HostMemoryRequest(0x1001u)), Status::invalid_parameter);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  CHECK_STATUS(chunk.open(mib, upload), Status::ok);
  CHECK_STATUS(chunk.open(mib, upload), Status::invalid_state);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes + 1u);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  CHECK_STATUS(chunk.release(), Status::ok);
  rig.check_only_the_link_is_left(); }

/* Nothing falls back to guest RAM: a device with no window is the kernel's
 * STATUS_NOT_SUPPORTED, and a window with no room for the chunk its
 * STATUS_INVALID_PARAMETER, with nothing held either way. The window's own
 * size still fits. */
TEST(host_window_chunk, a_chunk_the_window_cannot_hold_is_refused)
{ { Rig rig;
    HostWindowChunk chunk(rig.connection(), rig.transport());
    CHECK_STATUS(chunk.open(mib, upload), Status::not_supported);
    CHECK(chunk.invariants_hold());
    CHECK_EQ(chunk.resource_id(), 0u);
    rig.check_only_the_link_is_left(); }
  { Rig rig;
    rig.kmt().open_host_window(window_bytes);
    HostWindowChunk larger(rig.connection(), rig.transport());
    CHECK_STATUS(larger.open(window_bytes + 1u, upload), Status::invalid_parameter);
    CHECK_EQ(larger.resource_id(), 0u);
    HostWindowChunk whole(rig.connection(), rig.transport());
    CHECK_STATUS(whole.open(window_bytes, upload), Status::ok);
    CHECK_EQ(whole.resource_id(), MockKmt::first_window_resource);
    HostWindowChunk past(rig.connection(), rig.transport());
    CHECK_STATUS(past.open(4096u, upload), Status::invalid_parameter);
    CHECK(past.view() == nullptr);
    CHECK_EQ(rig.kmt().window_chunks(), 1u);
    CHECK_STATUS(whole.release(), Status::ok);
    CHECK_STATUS(past.open(4096u, upload), Status::ok);
    CHECK_STATUS(past.release(), Status::ok);
    rig.check_only_the_link_is_left(); } }

/* A device that will back the chunk nowhere, and any other refusal of the
 * open, is the kernel's answer, with nothing held. */
TEST(host_window_chunk, the_kernels_refusal_is_the_answer_and_nothing_is_held)
{ { Rig rig;
    HostWindowChunk chunk(rig.connection(), rig.transport());
    rig.kmt().fail_at(MockKmt::Call::escape, 1u, MockKmt::not_supported);
    CHECK_STATUS(chunk.open(mib, upload), Status::not_supported);
    CHECK(chunk.invariants_hold());
    CHECK_EQ(chunk.resource_id(), 0u);
    CHECK(chunk.view() == nullptr);
    rig.check_only_the_link_is_left(); }
  { Rig rig;
    rig.kmt().open_host_window(window_bytes);
    rig.kmt().refuse_next_window_open(MockKmt::invalid_parameter);
    HostWindowChunk chunk(rig.connection(), rig.transport());
    CHECK_STATUS(chunk.open(mib, upload), Status::invalid_parameter);
    CHECK_EQ(chunk.bytes(), 0u);
    rig.kmt().fail_at(MockKmt::Call::escape, 1u, refused);
    CHECK_STATUS(chunk.open(mib, upload), Status::invalid_state);
    CHECK_EQ(chunk.resource_id(), 0u);
    rig.check_only_the_link_is_left(); } }

/* Success with bytes that are not an answer - the request left as it was, a
 * size the kernel does not grant in pages, an address off a page, less than
 * was asked, another operation - is malformed and nothing is kept. */
TEST(host_window_chunk, an_answer_that_does_not_answer_the_open_is_malformed)
{ { Rig rig;
    rig.kmt().open_host_window(window_bytes);
    HostWindowChunk chunk(rig.connection(), rig.transport());
    rig.kmt().zero_next(MockKmt::Call::escape);
    CHECK_STATUS(chunk.open(mib, upload), Status::malformed);
    CHECK(chunk.invariants_hold());
    CHECK_EQ(chunk.resource_id(), 0u);
    rig.check_only_the_link_is_left(); }
  struct Case { std::uint64_t bytes; std::uint64_t address; };
  const Case cases[] = {{mib + 1u, 0x10000u}, {mib, 0x10010u}, {mib - 4096u, 0x10000u}, {mib, 0u}};
  for (const Case &one : cases) {
      Rig rig;
      rig.kmt().open_host_window(window_bytes);
      HostWindowChunk chunk(rig.connection(), rig.transport());
      script_open_answer(rig.kmt(), mib, MockKmt::first_window_resource, one.bytes, one.address);
      CHECK_STATUS(chunk.open(mib, upload), Status::malformed);
      CHECK(chunk.invariants_hold());
      CHECK(chunk.view() == nullptr);
      const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
      CHECK_STATUS(chunk.release(), Status::ok);
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  }
  { Rig rig;
    rig.kmt().open_host_window(window_bytes);
    HostWindowChunk chunk(rig.connection(), rig.transport());
    std::uint8_t answer[HostWindowRecord::encoded_size] = {};
    CHECK_STATUS(HostWindowRecord(HostWindowRecord::Operation::close, 0u, 7u, HostMemoryRequest()).encode(answer, sizeof answer),
                 Status::ok);
    rig.kmt().answer_next_escape(answer, sizeof answer);
    CHECK_STATUS(chunk.open(mib, upload), Status::malformed);
    CHECK_EQ(chunk.resource_id(), 0u); } }

/* A close the kernel refuses, or answers with something else, is what release
 * says; nothing is held after either way. */
TEST(host_window_chunk, a_refused_or_misanswered_close_still_lets_the_chunk_go)
{ { Rig rig;
    rig.kmt().open_host_window(window_bytes);
    HostWindowChunk chunk(rig.connection(), rig.transport());
    CHECK_STATUS(chunk.open(mib, upload), Status::ok);
    rig.kmt().fail_at(MockKmt::Call::escape, 1u, refused);
    CHECK_STATUS(chunk.release(), Status::invalid_state);
    CHECK(chunk.invariants_hold());
    CHECK_EQ(chunk.resource_id(), 0u);
    const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
    CHECK_STATUS(chunk.release(), Status::ok);
    CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes); }
  { Rig rig;
    rig.kmt().open_host_window(window_bytes);
    HostWindowChunk chunk(rig.connection(), rig.transport());
    CHECK_STATUS(chunk.open(mib, upload), Status::ok);
    std::uint8_t answer[HostWindowRecord::encoded_size] = {};
    CHECK_STATUS(HostWindowRecord(HostWindowRecord::Operation::close, 0u, chunk.resource_id() + 1u, HostMemoryRequest())
                     .encode(answer, sizeof answer),
                 Status::ok);
    rig.kmt().answer_next_escape(answer, sizeof answer);
    CHECK_STATUS(chunk.release(), Status::malformed);
    CHECK(chunk.view() == nullptr);
    CHECK(chunk.invariants_hold()); } }

/* On a faulted transport the kernel refuses a close and closes the chunk with
 * the context: nothing is sent, and the context's teardown frees it. */
TEST(host_window_chunk, release_on_a_faulted_transport_leaves_the_close_to_the_kernel)
{ Rig rig;
  rig.kmt().open_host_window(window_bytes);
  HostWindowChunk chunk(rig.connection(), rig.transport());
  CHECK_STATUS(chunk.open(mib, upload), Status::ok);
  rig.fault_the_transport();
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(chunk.release(), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  CHECK(chunk.invariants_hold());
  CHECK_EQ(chunk.resource_id(), 0u);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  rig.transport().teardown();
  CHECK_EQ(rig.kmt().window_chunks(), 0u); }

TEST(host_window_chunk, the_destructor_closes_what_is_held)
{ Rig rig;
  rig.kmt().open_host_window(window_bytes);
  { HostWindowChunk chunk(rig.connection(), rig.transport());
    CHECK_STATUS(chunk.open(mib, upload), Status::ok);
    CHECK_EQ(rig.kmt().window_chunks(), 1u); }
  rig.check_only_the_link_is_left(); }

TEST(host_window_chunk, invariants_want_a_resource_whole_pages_and_a_view_together)
{ Rig rig;
  rig.kmt().open_host_window(window_bytes);
  HostWindowChunk chunk(rig.connection(), rig.transport());
  CHECK_STATUS(chunk.open(mib, upload), Status::ok);
  CHECK(chunk.invariants_hold());
  HostWindowChunkProbe::claim_bytes(chunk, mib + 1u);
  CHECK(!chunk.invariants_hold());
  HostWindowChunkProbe::without_bytes(chunk);
  CHECK(!chunk.invariants_hold());
  HostWindowChunkProbe::claim_bytes(chunk, mib);
  CHECK(chunk.invariants_hold());
  HostWindowChunkProbe::without_view(chunk);
  CHECK(!chunk.invariants_hold());
  /* Nothing held: neither a size nor a view may be left. */
  HostWindowChunkProbe::without_resource(chunk);
  CHECK(!chunk.invariants_hold());
  HostWindowChunkProbe::without_bytes(chunk);
  CHECK(chunk.invariants_hold());
  HostWindowChunkProbe::view_at(chunk, rig.kmt().window_base());
  CHECK(!chunk.invariants_hold());
  HostWindowChunkProbe::without_view(chunk);
  CHECK(chunk.invariants_hold()); }
