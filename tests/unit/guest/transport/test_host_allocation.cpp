/* One allocation of host GPU memory a process holds (docs/host-memory.md),
 * over MockKmt playing the kernel and ScriptedHost the host: a window chunk
 * of the memory asked for, imported into the context it was opened on, let
 * go import first, and nothing held after any refusal. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/host_allocation.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

namespace av {

/* The parts' own probes, for a part broken on its own. */
struct HostWindowChunkProbe {
    static void claim_bytes(HostWindowChunk &chunk, std::uint64_t bytes) { chunk.bytes_ = bytes; }
};

struct BlobImportProbe {
    static void refused_and_held(BlobImport &import) { import.host_status_ = 1u; }
};

struct HostAllocationProbe {
    static HostWindowChunk &chunk(HostAllocation &allocation) { return allocation.chunk_; }
    static BlobImport      &import(HostAllocation &allocation) { return allocation.import_; }
    /* The chunk held with no import, and an import held with no chunk: what
     * open and close never leave. */
    static void forget_import(HostAllocation &allocation) { allocation.import_.forget(); }
    static Status drop_chunk(HostAllocation &allocation) { return allocation.chunk_.release(); }
};

}  // namespace av

using av::BlobImportAnswer;
using av::CommandHeader;
using av::GuestConnection;
using av::HostAllocation;
using av::HostAllocationProbe;
using av::HostMemoryRequest;
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
constexpr std::uint32_t link_objects = 7u;
const HostMemoryRequest vulkan(HostMemoryRequest::Kind::vulkan, 4u);

class Rig {
public:
    Rig()
        : adapter_(0u), connection_(kmt_), host_(kmt_), staging_{},
          transport_(connection_, SubApi::vulkan, 12u, reply_bytes, staging_, sizeof staging_),
          allocation_(connection_, transport_)
    {
        kmt_.add_adapter(av, 3200u);
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter_, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter_), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
        kmt_.open_host_window(64u * mib);
    }
    ~Rig() { CHECK_EQ(kmt_.misuses(), 0u); }

    MockKmt        &kmt() { return kmt_; }
    GuestConnection &connection() { return connection_; }
    ScriptedHost   &host() { return host_; }
    KmtTransport   &transport() { return transport_; }
    HostAllocation &allocation() { return allocation_; }

    void check_nothing_is_held(std::uint32_t kernels = 0u)
    {
        CHECK(allocation_.invariants_hold());
        CHECK(!allocation_.held());
        CHECK_EQ(allocation_.bytes(), 0u);
        CHECK(allocation_.view() == nullptr);
        CHECK_EQ(allocation_.handle(), 0u);
        CHECK_EQ(kmt_.window_chunks(), kernels);
        CHECK_EQ(kmt_.open_objects(), link_objects);
    }

private:
    MockKmt         kmt_;
    std::uint32_t   adapter_;
    GuestConnection connection_;
    ScriptedHost    host_;
    std::uint8_t    staging_[1024];
    KmtTransport    transport_;
    HostAllocation  allocation_;
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

struct ReleaseWitness {
    MockKmt      *kmt;
    std::uint32_t chunks_at_release;
};

void witness_release(void *context)
{
    ReleaseWitness &witness = *static_cast<ReleaseWitness *>(context);
    witness.chunks_at_release = witness.kmt->window_chunks();
}

}  // namespace

TEST(host_allocation, opens_a_chunk_of_the_memory_asked_and_imports_it_whole)
{ Rig rig;
  rig.check_nothing_is_held();
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(rig.allocation().open(3u * mib + 1u, vulkan), Status::ok);
  CHECK(rig.allocation().invariants_hold());
  CHECK(rig.allocation().held());
  CHECK_EQ(rig.allocation().bytes(), 3u * mib + 4096u);
  CHECK(rig.allocation().view() == rig.kmt().window_base());
  CHECK_EQ(rig.allocation().handle(), imported_memory);
  CHECK_EQ(rig.allocation().host_status(), 0u);
  CHECK_EQ(rig.kmt().window_request(MockKmt::first_window_resource).encoded(), vulkan.encoded());
  /* The import names the chunk the kernel made. */
  CHECK_EQ(rig.kmt().commands(), 1u);
  CHECK_EQ(rig.kmt().command_id(0u), CommandHeader::import_blob_id);
  CHECK_EQ(u32_at(rig.kmt().command_bytes(0u) + CommandHeader::encoded_size, 0u), MockKmt::first_window_resource);
  /* What the process writes through the view is the window's. */
  rig.allocation().view()[4096u] = 0x3Cu;
  CHECK_EQ(rig.kmt().window_base()[4096u], 0x3Cu);
  /* A second open is refused and asks nothing. */
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::invalid_state);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  CHECK_EQ(rig.kmt().commands(), 1u);
  /* Close releases the import while the chunk is still the kernel's. */
  ReleaseWitness witness = {&rig.kmt(), 0xFFu};
  rig.kmt().play_at_wait(1u, &witness_release, &witness);
  rig.host().nop();
  CHECK_STATUS(rig.allocation().close(), Status::ok);
  CHECK_EQ(witness.chunks_at_release, 1u);
  CHECK_EQ(rig.kmt().command_id(1u), CommandHeader::release_blob_id);
  CHECK_EQ(u64_at(rig.kmt().command_bytes(1u) + CommandHeader::encoded_size, 0u), imported_memory);
  rig.check_nothing_is_held();
  /* Closed twice, nothing is asked. */
  const std::uint32_t after = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(rig.allocation().close(), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), after);
  CHECK_EQ(rig.kmt().commands(), 2u); }

/* The process's own pages lent and imported: the view is the process's
 * address, the import names the kernel's resource, and a refusal on either
 * side holds nothing (docs/host-memory.md, "Application memory"). */
TEST(host_allocation, adopts_the_processs_own_pages_and_imports_them)
{ Rig rig;
  alignas(4096) static std::uint8_t pages[2u * 4096u];
  const HostMemoryRequest readback(HostMemoryRequest::Kind::readback, 0u);
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(rig.allocation().adopt(pages, sizeof pages, readback), Status::ok);
  CHECK(rig.allocation().invariants_hold());
  CHECK(rig.allocation().view() == pages);
  CHECK_EQ(rig.allocation().bytes(), sizeof pages);
  CHECK_EQ(rig.allocation().handle(), imported_memory);
  CHECK_EQ(u32_at(rig.kmt().command_bytes(0u) + CommandHeader::encoded_size, 0u), MockKmt::first_window_resource);
  CHECK_STATUS(rig.allocation().adopt(pages, sizeof pages, readback), Status::invalid_state);
  rig.host().nop();
  CHECK_STATUS(rig.allocation().close(), Status::ok);
  rig.check_nothing_is_held();
  CHECK_STATUS(rig.allocation().adopt(pages + 1, 4096u, readback), Status::invalid_parameter);
  rig.host().blob_import(BlobImportAnswer::executor_refused, 0u);
  CHECK_STATUS(rig.allocation().adopt(pages, sizeof pages, readback), Status::invalid_parameter);
  rig.check_nothing_is_held(); }

/* A chunk the kernel will not give is its answer, and nothing is imported. */
TEST(host_allocation, a_refused_chunk_imports_nothing)
{ { Rig rig;
    rig.kmt().open_host_window(0u);
    CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::not_supported);
    CHECK_EQ(rig.kmt().commands(), 0u);
    rig.check_nothing_is_held(); }
  { Rig rig;
    CHECK_STATUS(rig.allocation().open(mib, HostMemoryRequest()), Status::invalid_parameter);
    CHECK_EQ(rig.kmt().window_opens(), 0u);
    rig.kmt().fail_at(MockKmt::Call::escape, 1u, refused);
    CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::invalid_state);
    CHECK_EQ(rig.kmt().commands(), 0u);
    rig.check_nothing_is_held(); } }

/* An import the host refuses closes the chunk it was made for, keeps the
 * host's word, and the allocation can open again. */
TEST(host_allocation, an_import_the_host_refuses_closes_the_chunk)
{ Rig rig;
  rig.host().blob_import(BlobImportAnswer::executor_refused, 0u);
  CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::invalid_parameter);
  CHECK_EQ(rig.allocation().host_status(), static_cast<std::uint32_t>(BlobImportAnswer::executor_refused));
  CHECK_EQ(rig.kmt().commands(), 1u);   /* nothing to release */
  rig.check_nothing_is_held();
  CHECK_EQ(rig.kmt().window_opens(), 1u);
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::ok);
  CHECK_EQ(rig.allocation().host_status(), 0u);
  rig.host().nop();
  CHECK_STATUS(rig.allocation().close(), Status::ok);
  rig.check_nothing_is_held(); }

/* Every part is let go whatever the one before answered, and the first
 * failure is what close says. */
TEST(host_allocation, close_lets_every_part_go_and_answers_the_first_failure)
{ { Rig rig;
    rig.host().blob_import(0u, imported_memory);
    CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::ok);
    rig.host().blob_import(0u, 1u);   /* bytes where the release answers nothing */
    CHECK_STATUS(rig.allocation().close(), Status::malformed);
    rig.check_nothing_is_held(); }
  { Rig rig;
    rig.host().blob_import(0u, imported_memory);
    CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::ok);
    rig.host().nop();
    rig.kmt().fail_at(MockKmt::Call::escape, 1u, refused);   /* the chunk's close */
    CHECK_STATUS(rig.allocation().close(), Status::invalid_state);
    rig.check_nothing_is_held(1u); } }

/* A context whose device was lost forgets the import - its context frees it
 * on the host - and lets the chunk go; the chunk's refusal is the answer. */
TEST(host_allocation, abandon_sends_the_host_nothing_and_closes_the_chunk)
{ Rig rig;
  CHECK_STATUS(rig.allocation().abandon(), Status::ok);
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::ok);
  CHECK_STATUS(rig.allocation().abandon(), Status::ok);
  CHECK_EQ(rig.kmt().commands(), 1u);
  rig.check_nothing_is_held();
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::ok);
  rig.kmt().fail_at(MockKmt::Call::escape, 1u, refused);
  CHECK_STATUS(rig.allocation().abandon(), Status::invalid_state);
  CHECK_EQ(rig.kmt().commands(), 2u);
  rig.check_nothing_is_held(1u); }

TEST(host_allocation, the_destructor_closes_what_is_held)
{ Rig rig;
  { HostAllocation held(rig.connection(), rig.transport());
    rig.host().blob_import(0u, imported_memory);
    CHECK_STATUS(held.open(mib, vulkan), Status::ok);
    CHECK_EQ(rig.kmt().window_chunks(), 1u);
    rig.host().nop(); }
  CHECK_EQ(rig.kmt().commands(), 2u);
  CHECK_EQ(rig.kmt().command_id(1u), CommandHeader::release_blob_id);
  rig.check_nothing_is_held(); }

TEST(host_allocation_invariants, a_chunk_without_its_import_or_an_import_without_its_chunk_is_caught)
{ { Rig rig;
    rig.host().blob_import(0u, imported_memory);
    CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::ok);
    CHECK(rig.allocation().invariants_hold());
    HostAllocationProbe::forget_import(rig.allocation());
    CHECK(!rig.allocation().invariants_hold());
    CHECK_STATUS(rig.allocation().close(), Status::ok);
    rig.check_nothing_is_held(); }
  { Rig rig;
    rig.host().blob_import(0u, imported_memory);
    CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::ok);
    CHECK_STATUS(HostAllocationProbe::drop_chunk(rig.allocation()), Status::ok);
    CHECK(!rig.allocation().invariants_hold());
    HostAllocationProbe::forget_import(rig.allocation());
    CHECK(rig.allocation().invariants_hold()); } }

/* A part broken on its own breaks the allocation: a chunk whose size is no
 * page count, and an import that both holds memory and keeps a refusal. */
TEST(host_allocation_invariants, a_broken_chunk_or_a_broken_import_is_caught)
{ Rig rig;
  rig.host().blob_import(0u, imported_memory);
  CHECK_STATUS(rig.allocation().open(mib, vulkan), Status::ok);
  av::HostWindowChunkProbe::claim_bytes(HostAllocationProbe::chunk(rig.allocation()), mib + 1u);
  CHECK(!rig.allocation().invariants_hold());
  av::HostWindowChunkProbe::claim_bytes(HostAllocationProbe::chunk(rig.allocation()), mib);
  CHECK(rig.allocation().invariants_hold());
  av::BlobImportProbe::refused_and_held(HostAllocationProbe::import(rig.allocation()));
  CHECK(!rig.allocation().invariants_hold());
  rig.host().nop();
  CHECK_STATUS(rig.allocation().close(), Status::ok);
  rig.check_nothing_is_held(); }
