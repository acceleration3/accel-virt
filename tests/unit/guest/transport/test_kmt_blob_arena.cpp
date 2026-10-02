/* The room an OpenGL context lends bulk data in (feature 050, research.md §5),
 * over MockKmt: a chunk bound to the context's own transport - guest RAM on a
 * device with no window, else a chunk of its host window - and imported whole
 * with IMPORT_BLOB, lent through that transport's blobs() only
 * while it is held, and let go import first - the device refuses to unbind a
 * chunk it still has imported. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct KmtBlobArenaProbe {
    static void lend_without_chunk(KmtBlobArena &arena) { arena.bytes_ = 0u; }
    static void claim_bytes(KmtBlobArena &arena, std::uint64_t bytes) { arena.bytes_ = bytes; }
};

}  // namespace av

using av::ClientBlobLease;
using av::CommandHeader;
using av::GuestConnection;
using av::KmtBlobArena;
using av::KmtBlobArenaProbe;
using av::KmtTransport;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedHost;
using av::Status;
using av::SubApi;

namespace {

const PciIdentity av(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint64_t mib = std::uint64_t{1} << 20u;
constexpr std::uint64_t imported_memory = 0x0000600000000077ull;
constexpr std::uint32_t refused = 0xC0000001u;
constexpr std::uint32_t reply_bytes = 4096u;
/* The connection's device and queue, the transport's two allocations, its DMA
 * mapping and its context, with the adapter. */
constexpr std::uint32_t link_objects = 7u;

class Rig {
public:
    Rig()
        : adapter_(0u), connection_(kmt_), host_(kmt_), staging_{},
          transport_(connection_, SubApi::opengl, 12u, reply_bytes, staging_, sizeof staging_),
          arena_(connection_, transport_)
    {
        kmt_.add_adapter(av, 3200u);
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter_, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter_), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
        CHECK(arena_.invariants_hold());
    }
    ~Rig() { CHECK_EQ(kmt_.misuses(), 0u); }

    MockKmt      &kmt() { return kmt_; }
    ScriptedHost &host() { return host_; }
    KmtTransport &transport() { return transport_; }
    KmtBlobArena &arena() { return arena_; }

    /* The host takes the import, then answers its release with nothing. */
    void host_takes_the_import() { host_.blob_import(0u, imported_memory); }

    /* `kernels` is the chunks the arena let go of that the kernel still
     * holds - a close it refused, or never got on a lost context - which it
     * closes with the context. */
    void check_nothing_is_held(std::uint32_t kernels = 0u)
    {
        CHECK(arena_.invariants_hold());
        CHECK(!arena_.lending());
        CHECK_EQ(arena_.bytes(), 0u);
        CHECK(!transport_.blobs().ready());
        CHECK(!transport_.blobs().take(1u).taken());
        CHECK_EQ(kmt_.open_objects(), link_objects);
        CHECK_EQ(kmt_.ram_chunks(), kernels);
    }

private:
    MockKmt         kmt_;
    std::uint32_t   adapter_;
    GuestConnection connection_;
    ScriptedHost    host_;
    std::uint8_t    staging_[1024];
    KmtTransport    transport_;
    KmtBlobArena    arena_;
};

std::uint32_t u32_at(const std::uint8_t *bytes, std::uint32_t at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

std::uint64_t u64_at(const std::uint8_t *bytes, std::uint32_t at)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

/* The bindings a release saw, for the check that the import went first. */
struct ReleaseWitness {
    MockKmt      *kmt;
    std::uint32_t bindings_at_release;
};

void witness_release(void *context)
{
    ReleaseWitness &witness = *static_cast<ReleaseWitness *>(context);
    witness.bindings_at_release = witness.kmt->ram_chunks();
}

/* The window chunks held when the release was waited for. */
void witness_window_release(void *context)
{
    ReleaseWitness &witness = *static_cast<ReleaseWitness *>(context);
    witness.bindings_at_release = witness.kmt->window_chunks();
}

}  // namespace

TEST(kmt_blob_arena, lends_nothing_before_it_is_opened)
{ Rig rig;
  CHECK(!rig.arena().lending());
  CHECK(!rig.transport().blobs().ready());
  CHECK(!rig.transport().blobs().take(4096u).taken());
  CHECK_STATUS(rig.arena().close(), Status::ok);
  CHECK_EQ(rig.kmt().commands(), 0u);
  rig.check_nothing_is_held(); }

TEST(kmt_blob_arena, opens_a_bound_chunk_imported_whole_and_lends_inside_its_view)
{ Rig rig;
  rig.host_takes_the_import();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  CHECK(rig.arena().invariants_hold());
  CHECK(rig.arena().lending());
  CHECK_EQ(rig.arena().bytes(), KmtBlobArena::default_bytes);
  CHECK_EQ(KmtBlobArena::default_bytes, 16u * mib);
  /* One chunk of guest RAM - the device has no window - on this transport's
   * context. */
  CHECK_EQ(rig.kmt().ram_chunks(), 1u);
  CHECK_EQ(rig.kmt().ram_chunk_pages(0u), 16u * mib / MockKmt::page_bytes);
  /* The import asked for all of it, at its start, and needs no memory type. */
  CHECK_EQ(rig.kmt().commands(), 1u);
  CHECK_EQ(rig.kmt().command_id(0u), CommandHeader::import_blob_id);
  const std::uint8_t *const request = rig.kmt().command_bytes(0u) + CommandHeader::encoded_size;
  CHECK_EQ(u32_at(request, 0u), MockKmt::first_chunk_resource);
  CHECK_EQ(u64_at(request, 8u), 0u);
  CHECK_EQ(u64_at(request, 16u), 16u * mib);
  CHECK_EQ(u32_at(request, 24u), 0u);
  /* Leases are the chunk's own bytes, at their offset from its start. */
  av::ClientBlobArena &blobs = rig.transport().blobs();
  CHECK(blobs.ready());
  const std::uint8_t *const view = rig.kmt().ram_chunk(0u);
  ClientBlobLease first = blobs.take(4096u);
  ClientBlobLease second = blobs.take(100u);
  CHECK(first.taken());
  CHECK_EQ(first.offset(), 0u);
  CHECK(static_cast<const void *>(first.data()) == static_cast<const void *>(view));
  CHECK_EQ(second.offset(), 4096u);
  CHECK(static_cast<const void *>(second.data()) == static_cast<const void *>(view + 4096u));
  std::memset(second.data(), 0xA5, second.bytes());
  CHECK_EQ(view[4096u + 99u], 0xA5u);
  CHECK(rig.arena().invariants_hold());
  /* A request past the room below the error word is no lease, and takes
   * nothing. */
  CHECK(!blobs.take(16u * mib - KmtBlobArena::error_word_bytes).taken());
  CHECK_EQ(blobs.used(), 4196u);
  /* Only the top lease rewinds; retiring returns them all. */
  blobs.give_back(first);
  CHECK_EQ(blobs.used(), 4196u);
  blobs.give_back(second);
  CHECK_EQ(blobs.used(), 4096u);
  blobs.retire_all();
  CHECK_EQ(blobs.used(), 0u);
  CHECK(!blobs.take(16u * mib).taken());
  CHECK(blobs.take(16u * mib - KmtBlobArena::error_word_bytes).taken());
  CHECK(rig.arena().invariants_hold());
  /* A second open is refused and asks nothing. */
  CHECK_STATUS(rig.arena().open(0u), Status::invalid_state);
  CHECK_EQ(rig.kmt().commands(), 1u);
  CHECK_EQ(rig.kmt().ram_chunks(), 1u);
  rig.host().nop();
  CHECK_STATUS(rig.arena().close(), Status::ok);
  CHECK_EQ(rig.kmt().commands(), 2u);
  CHECK_EQ(rig.kmt().command_id(1u), CommandHeader::release_blob_id);
  CHECK_EQ(u64_at(rig.kmt().command_bytes(1u) + CommandHeader::encoded_size, 0u), imported_memory);
  rig.check_nothing_is_held(); }

/* The size is whole pages, at least the default, and at most 1 GiB; a
 * request past that asks the Kmt nothing. */
TEST(kmt_blob_arena, sizes_are_whole_pages_between_the_default_and_the_ceiling)
{ struct Case { std::uint64_t asked; std::uint64_t made; };
  const Case cases[] = {{1u, 16u * mib}, {16u * mib - 1u, 16u * mib}, {16u * mib, 16u * mib},
                        {16u * mib + 1u, 16u * mib + 4096u}, {100u * mib + 5u, 100u * mib + 4096u},
                        {KmtBlobArena::max_bytes, 1024u * mib}};
  for (const Case &one : cases) {
      Rig rig;
      rig.host_takes_the_import();
      CHECK_STATUS(rig.arena().open(one.asked), Status::ok);
      CHECK_EQ(rig.arena().bytes(), one.made);
      CHECK_EQ(rig.kmt().ram_chunk_pages(0u), one.made / MockKmt::page_bytes);
      CHECK(!rig.transport().blobs().take(static_cast<std::size_t>(one.made)).taken());
      CHECK(rig.transport().blobs().take(static_cast<std::size_t>(one.made - KmtBlobArena::error_word_bytes)).taken());
      CHECK(rig.arena().invariants_hold());
      rig.host().nop();
      CHECK_STATUS(rig.arena().close(), Status::ok);
      rig.check_nothing_is_held();
  }
  const std::uint64_t pasts[] = {KmtBlobArena::max_bytes + 1u, KmtBlobArena::max_bytes + 4096u,
                                 ~std::uint64_t{0}, ~std::uint64_t{0} - 4094u};
  for (const std::uint64_t past : pasts) {
      Rig rig;
      CHECK_STATUS(rig.arena().open(past), Status::invalid_parameter);
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), 2u);   /* the transport's own */
      CHECK_EQ(rig.kmt().commands(), 0u);
      rig.check_nothing_is_held();
  } }

/* The kernel's refusal of the chunk answers its status and leaves nothing
 * held: no chunk, no import, nothing lent. */
TEST(kmt_blob_arena, a_refused_chunk_leaves_nothing_held)
{ Rig rig;
  const std::uint32_t before = rig.kmt().calls(MockKmt::Call::escape);
  rig.kmt().fail_at(MockKmt::Call::escape, 1u, refused);
  CHECK_STATUS(rig.arena().open(0u), Status::invalid_state);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), before + 1u);
  CHECK_EQ(rig.kmt().commands(), 0u);
  rig.check_nothing_is_held();
  /* And it opens once the Kmt allows it. */
  rig.host_takes_the_import();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  rig.host().nop();
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

/* The host's refusal of the import and an empty answer each close the chunk
 * they made; a context lost at the import leaves it to the kernel. */
TEST(kmt_blob_arena, an_import_the_host_does_not_take_leaves_nothing_held)
{ { Rig rig;
    rig.host().blob_import(av::BlobImportAnswer::executor_refused, 0u);
    CHECK_STATUS(rig.arena().open(0u), Status::invalid_parameter);
    CHECK_EQ(rig.kmt().commands(), 1u);   /* nothing to release */
    rig.check_nothing_is_held(); }
  { Rig rig;
    rig.host().nop();
    CHECK_STATUS(rig.arena().open(0u), Status::not_supported);
    rig.check_nothing_is_held(); }
  { Rig rig;
    rig.kmt().withhold_answer_at(1u);
    CHECK_STATUS(rig.arena().open(0u), Status::invalid_state);
    CHECK(rig.transport().faulted());
    rig.check_nothing_is_held(1u); } }

TEST(kmt_blob_arena, close_releases_the_import_while_the_chunk_is_still_bound)
{ Rig rig;
  rig.host_takes_the_import();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  ReleaseWitness witness = {&rig.kmt(), 0xFFu};
  rig.kmt().play_at_wait(1u, &witness_release, &witness);
  rig.host().nop();
  CHECK_STATUS(rig.arena().close(), Status::ok);
  CHECK_EQ(witness.bindings_at_release, 1u);
  rig.check_nothing_is_held();
  /* Closed twice, nothing is asked. */
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes); }

/* Every part is let go whatever the one before answered, and the first failure
 * is what close says; the arena can open again afterwards. */
TEST(kmt_blob_arena, close_lets_every_part_go_and_answers_the_first_failure)
{ { Rig rig;
    rig.host_takes_the_import();
    CHECK_STATUS(rig.arena().open(0u), Status::ok);
    rig.host().blob_import(0u, 1u);   /* bytes where the release answers nothing */
    CHECK_STATUS(rig.arena().close(), Status::malformed);
    rig.check_nothing_is_held(); }
  { Rig rig;
    rig.host_takes_the_import();
    CHECK_STATUS(rig.arena().open(0u), Status::ok);
    rig.host().nop();
    rig.kmt().fail_at(MockKmt::Call::escape, 1u, refused);   /* the close */
    CHECK_STATUS(rig.arena().close(), Status::invalid_state);
    CHECK(!rig.arena().lending());
    CHECK(!rig.transport().blobs().ready());
    CHECK_EQ(rig.kmt().ram_chunks(), 1u);
    rig.host_takes_the_import();
    CHECK_STATUS(rig.arena().open(0u), Status::ok);
    rig.host().nop();
    CHECK_STATUS(rig.arena().close(), Status::ok);
    rig.check_nothing_is_held(1u); } }

/* The chunk's last eight bytes are the host's error word: zeroed at open,
 * which is what a host that publishes none leaves, read whole, and never
 * lent, so no lease the host writes into can overwrite it. */
TEST(kmt_blob_arena, keeps_the_last_word_for_the_hosts_errors)
{ Rig rig;
  CHECK_EQ(rig.arena().host_errors(), 0u);
  rig.host_takes_the_import();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  std::uint8_t *const chunk = rig.kmt().ram_chunk(0u);
  CHECK_EQ(rig.arena().host_errors(), 0u);
  const std::uint64_t word = 0x8000000000030201ull;
  std::memcpy(chunk + 16u * mib - 8u, &word, sizeof word);
  CHECK_EQ(rig.arena().host_errors(), word);
  av::ClientBlobArena &blobs = rig.transport().blobs();
  CHECK_EQ(blobs.room(), 16u * mib - 8u);
  const ClientBlobLease all = blobs.take(blobs.room());
  CHECK(all.taken());
  CHECK(static_cast<const std::uint8_t *>(all.data()) + all.bytes() == chunk + 16u * mib - 8u);
  rig.host().nop();
  CHECK_STATUS(rig.arena().close(), Status::ok);
  CHECK_EQ(rig.arena().host_errors(), 0u); }

TEST(kmt_blob_arena, the_invariant_catches_a_size_it_never_makes_and_lending_out_of_step)
{ Rig rig;
  rig.host_takes_the_import();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  CHECK(rig.arena().invariants_hold());
  KmtBlobArenaProbe::claim_bytes(rig.arena(), 16u * mib + 1u);
  CHECK(!rig.arena().invariants_hold());
  KmtBlobArenaProbe::claim_bytes(rig.arena(), KmtBlobArena::max_bytes + 4096u);
  CHECK(!rig.arena().invariants_hold());
  KmtBlobArenaProbe::claim_bytes(rig.arena(), 4096u);
  CHECK(!rig.arena().invariants_hold());
  /* Holding a chunk it no longer lends. */
  KmtBlobArenaProbe::claim_bytes(rig.arena(), 16u * mib);
  rig.transport().blobs().lend(nullptr, 0u);
  CHECK(!rig.arena().invariants_hold());
  rig.transport().blobs().lend(rig.kmt().ram_chunk(0u), 16u * mib);
  CHECK(rig.arena().invariants_hold());
  KmtBlobArenaProbe::lend_without_chunk(rig.arena());
  CHECK(!rig.arena().invariants_hold());
  KmtBlobArenaProbe::claim_bytes(rig.arena(), 16u * mib);
  CHECK(rig.arena().invariants_hold());
  rig.host().nop();
  CHECK_STATUS(rig.arena().close(), Status::ok); }

/* An arena abandoned with its context's device lost takes the lending back
 * and closes the chunk, but sends the host nothing: the import goes with the
 * host context. Abandoned twice, or never opened, nothing is asked. */
TEST(kmt_blob_arena, abandon_lets_the_chunk_go_without_a_host_call)
{ Rig rig;
  CHECK_STATUS(rig.arena().abandon(), Status::ok);
  rig.host_takes_the_import();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK_STATUS(rig.arena().abandon(), Status::ok);
  CHECK_EQ(rig.kmt().commands(), commands);
  rig.check_nothing_is_held();
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(rig.arena().abandon(), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  /* A refused close is what abandon answers, with nothing held all the same. */
  rig.host_takes_the_import();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  rig.kmt().fail_at(MockKmt::Call::escape, 1u, refused);
  CHECK_STATUS(rig.arena().abandon(), Status::invalid_state);
  rig.check_nothing_is_held(1u); }

/* An arena that is lending nothing has no memory to name. The OpenCL ICD
 * asks for this view to carve its buffers out of, and a context whose device
 * would not import one must get null rather than a pointer into whatever the
 * chunk last held. */
TEST(kmt_blob_arena, an_arena_that_lends_nothing_has_no_view)
{ Rig rig;
  CHECK(rig.arena().view() == nullptr);
  CHECK_EQ(rig.arena().bytes(), 0u);
  CHECK(rig.arena().invariants_hold()); }

/* On a device with a host window the arena is a chunk of it: imported and
 * lent the same way, with leases in the window's own memory, and closed with
 * the import released first. */
TEST(kmt_blob_arena, over_a_host_window_lends_the_windows_memory)
{ Rig rig;
  rig.kmt().open_host_window(64u * mib);
  rig.host_takes_the_import();
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  CHECK(rig.arena().invariants_hold());
  CHECK_EQ(rig.arena().bytes(), KmtBlobArena::default_bytes);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes + 1u);
  CHECK_EQ(rig.kmt().ram_chunks(), 0u);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  /* The import names the window chunk's resource, all of it. */
  CHECK_EQ(rig.kmt().commands(), 1u);
  const std::uint8_t *const request = rig.kmt().command_bytes(0u) + CommandHeader::encoded_size;
  CHECK_EQ(u32_at(request, 0u), MockKmt::first_window_resource);
  CHECK_EQ(u64_at(request, 16u), 16u * mib);
  CHECK(rig.arena().view() == rig.kmt().window_base());
  ClientBlobLease lease = rig.transport().blobs().take(64u);
  CHECK(lease.taken());
  CHECK(static_cast<const void *>(lease.data()) == static_cast<const void *>(rig.kmt().window_base()));
  std::memset(lease.data(), 0x77, lease.bytes());
  CHECK_EQ(rig.kmt().window_base()[63u], 0x77u);
  ReleaseWitness witness = {&rig.kmt(), 0xFFu};
  rig.kmt().play_at_wait(1u, &witness_window_release, &witness);
  rig.host().nop();
  CHECK_STATUS(rig.arena().close(), Status::ok);
  CHECK_EQ(witness.bindings_at_release, 1u);
  CHECK_EQ(rig.kmt().commands(), 2u);
  CHECK_EQ(rig.kmt().command_id(1u), CommandHeader::release_blob_id);
  rig.check_nothing_is_held();
  CHECK_EQ(rig.kmt().window_chunks(), 0u); }

/* A window with no room for the arena leaves it to guest RAM, as before. */
TEST(kmt_blob_arena, a_full_window_leaves_the_arena_to_guest_ram)
{ Rig rig;
  rig.kmt().open_host_window(8u * mib);
  rig.host_takes_the_import();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK_EQ(rig.kmt().ram_chunks(), 1u);
  CHECK(rig.arena().view() == rig.kmt().ram_chunk(0u));
  rig.host().nop();
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

/* An abandoned window arena on a faulted context sends nothing: the kernel
 * closes the chunk with the context. */
TEST(kmt_blob_arena, an_abandoned_window_arena_is_closed_by_the_kernel)
{ Rig rig;
  rig.kmt().open_host_window(64u * mib);
  rig.host_takes_the_import();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  rig.kmt().withhold_answer_at(1u);
  av::WireWriter request = rig.transport().request_writer();
  request.write_u32(1u);
  av::WireReader reply(nullptr, 0u);
  CHECK(rig.transport().send_sync(1u, request, reply) != av::TransportResult::ok);
  rig.transport().end_call();
  CHECK(rig.transport().faulted());
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(rig.arena().abandon(), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  CHECK(!rig.arena().lending());
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  rig.transport().teardown();
  CHECK_EQ(rig.kmt().window_chunks(), 0u); }
