/* One host GPU's single source of the memory a guest CPU and a host GPU share
 * (docs/host-memory.md), over an exporter whose memory types each test lays
 * out. What a caller acts on: which memory type a request is answered with
 * and how it is cached, the descriptor it is handed, refusals that leave
 * nothing behind, and when the memory goes back to the driver - only once
 * the device's hold and every import have let go. */
#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "executor/exported_memory.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/memory_exporter.hpp"
#include "executor/source_memory.hpp"
#include "harness/executor/host_memory_source_probe.hpp"
#include "harness/executor/mock_memory_exporter.hpp"
#include "executor/vulkan/guest_memory_types.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void shift_live_count(ObjectPool<T> &pool, std::int32_t by)
    {
        pool.live_ = static_cast<std::uint32_t>(static_cast<std::int32_t>(pool.live_) + by);
    }
};

}  // namespace av

using av::ExportedMemory;
using av::HostMemoryRequest;
using av::HostMemorySource;
using av::HostMemorySourceProbe;
using av::MockMemoryExporter;
using av::MockPlatform;
using av::SourceMemory;
using av::Status;

namespace {

constexpr std::uint32_t local = MockMemoryExporter::device_local;
constexpr std::uint32_t visible = MockMemoryExporter::host_visible;
constexpr std::uint32_t coherent = MockMemoryExporter::host_coherent;
constexpr std::uint32_t cached = MockMemoryExporter::host_cached;
constexpr std::uint64_t page = 4096u;
constexpr std::uint32_t domain = 1u;

const HostMemoryRequest upload(HostMemoryRequest::Kind::upload, 0u);
const HostMemoryRequest readback(HostMemoryRequest::Kind::readback, 0u);

HostMemoryRequest vulkan(std::uint32_t type)
{
    return HostMemoryRequest(HostMemoryRequest::Kind::vulkan, type);
}

/* The exporter outlives the source, which frees everything into it; the
 * platform outlives both, and fails the test on a block left behind. */
class Rig {
public:
    Rig() : platform(), exporter(), source(exporter, platform) {}

    /* A discrete GPU: device-local memory, uncached and cached system memory,
     * the resizable BAR, memory that is not coherent, and coherent memory
     * whose export a driver refuses. Mappable: 1, 2 and 3.
     *
     * A Vulkan guest sees 4 and 5 without host visibility, which leaves them
     * no flags at all, so the spec puts them first: its types are the host's
     * 4, 5, 0, 1, 2, 3. */
    static constexpr std::uint32_t guest_uncached = 3u;
    static constexpr std::uint32_t guest_cached = 4u;
    static constexpr std::uint32_t guest_bar = 5u;

    void discrete()
    {
        exporter.add_type(local, true);
        exporter.add_type(visible | coherent, true);
        exporter.add_type(visible | coherent | cached, true);
        exporter.add_type(local | visible | coherent, true);
        exporter.add_type(visible, true);
        exporter.add_type(visible | coherent, false);
    }

    /* `request`, `size` bytes under `key`: ok with the answer, whose descriptor
     * is closed here so a test judges only what it was. */
    Status allocate(std::uint64_t key, HostMemoryRequest request, AvHostMemory &memory, std::uint64_t size = page,
                    std::uint32_t in = domain)
    {
        memory = AvHostMemory{-1, 0u, 0u};
        const Status status = source.allocate(in, key, size, request, memory);
        if (memory.fd >= 0) ::close(memory.fd);
        return status;
    }

    MockPlatform       platform;
    MockMemoryExporter exporter;
    HostMemorySource   source;
};

std::uint32_t open_descriptors()
{
    std::uint32_t count = 0u;
    DIR *const    directory = ::opendir("/proc/self/fd");
    if (directory == nullptr) return 0u;
    while (::readdir(directory) != nullptr) count++;
    ::closedir(directory);
    return count;
}

}  // namespace

/* Only memory a guest can map and the GPU can export is offered: host
 * visible and coherent, and proved to export. A type that is neither is never
 * put to the exporter's probe, and the probe is asked once per type. */
TEST(host_memory_source_types, offers_coherent_exportable_types_and_asks_each_once)
{ Rig rig;
  rig.discrete();
  CHECK_EQ(rig.source.mappable_types(), 0xEu);
  CHECK_EQ(rig.exporter.export_questions(0u), 0u);
  CHECK_EQ(rig.exporter.export_questions(1u), 1u);
  CHECK_EQ(rig.exporter.export_questions(3u), 1u);
  CHECK_EQ(rig.exporter.export_questions(4u), 0u);
  CHECK_EQ(rig.exporter.export_questions(5u), 1u);
  CHECK_EQ(rig.source.mappable_types(), 0xEu);
  for (std::uint32_t type = 0u; type < 6u; type++) CHECK(rig.exporter.export_questions(type) <= 1u);
  CHECK_EQ(rig.exporter.allocations(), 0u);
  CHECK(rig.source.invariants_hold()); }

/* Whether an export imports back host-visible is asked once, of the types
 * the source maps and of no other: a type it does not map is never imported. */
TEST(host_memory_source_types, asks_whether_each_mapped_type_imports_visible_once)
{ Rig rig;
  rig.discrete();
  rig.exporter.hide_on_import(3u);
  CHECK_EQ(rig.source.mappable_types(), 0xEu);
  CHECK_EQ(rig.source.mappable_types(), 0xEu);
  constexpr std::uint32_t mapped[] = {1u, 2u, 3u}, unmapped[] = {0u, 4u, 5u};
  for (const std::uint32_t type : mapped) CHECK_EQ(rig.exporter.import_questions(type), 1u);
  for (const std::uint32_t type : unmapped) CHECK_EQ(rig.exporter.import_questions(type), 0u);
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, readback, memory), Status::ok);
  char            bytes[512] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  rig.source.report(writer);
  CHECK(std::strstr(writer.dump(), "visible_types=0x0000000000000006") != nullptr);
  CHECK(rig.source.invariants_hold());
  rig.source.free_domain(domain); }

/* Nothing is asked before something wants to know - a device with no
 * compositing context yet has no types - and a type that appears later is
 * asked then, while one already asked is not asked again. */
TEST(host_memory_source_types, a_type_appearing_later_is_asked_then_and_the_rest_are_not)
{ Rig rig;
  CHECK_EQ(rig.source.mappable_types(), 0u);
  rig.exporter.add_type(visible | coherent, true);
  CHECK_EQ(rig.source.mappable_types(), 0x1u);
  rig.exporter.add_type(visible | coherent | cached, true);
  CHECK_EQ(rig.source.mappable_types(), 0x3u);
  CHECK_EQ(rig.exporter.export_questions(0u), 1u);
  CHECK_EQ(rig.exporter.export_questions(1u), 1u); }

/* A driver with more types than a mask holds is asked about the first
 * thirty-two only: VK_MAX_MEMORY_TYPES is the most a request can name. */
TEST(host_memory_source_types, never_asks_past_the_thirty_two_a_request_can_name)
{ Rig rig;
  for (std::uint32_t type = 0u; type < MockMemoryExporter::max_types; type++)
      rig.exporter.add_type(visible | coherent, type % 2u == 0u);
  rig.exporter.claim_types(40u);
  CHECK_EQ(rig.source.mappable_types(), 0x55555555u);
  CHECK_EQ(rig.exporter.misuses(), 0u); }

/* Upload memory is the GPU's own through a resizable BAR, written combined;
 * the answer is a descriptor of its own, the size the exporter made. */
TEST(host_memory_source_choice, upload_prefers_the_bar_written_combined)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 3u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  CHECK_EQ(memory.size, page);
  CHECK_EQ(rig.source.live(), 1u);
  CHECK(rig.source.invariants_hold());
  rig.source.free(domain, 1u);
  CHECK_EQ(rig.exporter.live(), 0u); }

/* With no BAR - or none with room - upload falls back to uncached system
 * memory, then to anything mappable, and refuses only when every choice did:
 * a VRAM BAR without resizable BAR is 256 MiB. */
TEST(host_memory_source_choice, upload_falls_back_to_uncached_then_to_any_type)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 1u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  CHECK_EQ(rig.exporter.attempts(), 2u);

  /* Memory types are chosen afresh for each request. */
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(2u, upload, memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 1u);

  /* The BAR and uncached memory both full: the first mappable type, cached. */
  const std::uint32_t before = rig.exporter.attempts();
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(3u, upload, memory), Status::ok);
  CHECK_EQ(rig.exporter.attempts(), before + 2u);
  CHECK_EQ(rig.exporter.last_type(), 1u);
  CHECK(rig.source.invariants_hold());
  rig.source.free_domain(domain);
  CHECK_EQ(rig.exporter.live(), 0u); }

/* A BAR whose dma-buf imports back only as device-local memory - NVIDIA's,
 * which vkd3d-proton then refuses as an upload heap - is upload's last
 * choice, after every type an importer can map; a Vulkan request for it is
 * still answered with it, since the backend imports it as that type. */
TEST(host_memory_source_choice, upload_takes_a_bar_no_importer_can_map_last)
{ Rig rig;
  rig.discrete();
  rig.exporter.hide_on_import(3u);
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 1u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  CHECK_EQ(rig.exporter.attempts(), 1u);
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(2u, upload, memory), Status::ok);
  CHECK_EQ(rig.exporter.attempts(), 3u);
  CHECK_EQ(rig.exporter.last_type(), 3u);
  rig.exporter.fail_allocations(2u);
  CHECK_STATUS(rig.allocate(3u, upload, memory), Status::no_memory);
  CHECK_EQ(rig.exporter.attempts(), 5u);
  CHECK_STATUS(rig.allocate(4u, vulkan(Rig::guest_bar), memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 3u);
  CHECK(rig.source.invariants_hold());
  rig.source.free_domain(domain);
  CHECK_EQ(rig.exporter.live(), 0u); }

/* Readback's cached memory is passed over when it would not import mappable,
 * for any type that would, and taken when that had no room. */
TEST(host_memory_source_choice, readback_takes_cached_memory_no_importer_can_map_last)
{ Rig rig;
  rig.discrete();
  rig.exporter.hide_on_import(2u);
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, readback, memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 1u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(2u, readback, memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 2u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_BACK});
  const std::uint32_t before = rig.exporter.attempts();
  rig.exporter.fail_allocations(2u);
  CHECK_STATUS(rig.allocate(3u, readback, memory), Status::no_memory);
  CHECK_EQ(rig.exporter.attempts(), before + 2u);
  rig.source.free_domain(domain);
  CHECK_EQ(rig.exporter.live(), 0u); }

TEST(host_memory_source_choice, upload_refused_by_every_choice_is_no_memory_and_holds_nothing)
{ Rig rig;
  rig.exporter.add_type(visible | coherent | cached, true);
  rig.exporter.add_type(local | visible | coherent, true);
  rig.exporter.add_type(visible | coherent, true);
  AvHostMemory memory = {};
  /* Three choices, each its own type: the BAR, uncached, the first of any. */
  rig.exporter.fail_allocations(2u);
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  CHECK_EQ(rig.exporter.attempts(), 3u);
  CHECK_EQ(rig.exporter.last_type(), 0u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_BACK});
  rig.exporter.fail_allocations(3u);
  CHECK_STATUS(rig.allocate(2u, upload, memory), Status::no_memory);
  CHECK_EQ(rig.exporter.attempts(), 6u);
  CHECK_EQ(memory.fd, -1);
  CHECK_EQ(rig.source.live(), 1u);
  rig.source.free(domain, 1u);
  CHECK_EQ(rig.exporter.live(), 0u); }

/* Readback memory is cached, which only a CPU reading it at speed needs; with
 * none of it to spare, any mappable type. */
TEST(host_memory_source_choice, readback_prefers_cached_written_back_and_falls_back_to_any)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, readback, memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 2u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_BACK});
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(2u, readback, memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 1u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  rig.source.free_domain(domain);
  CHECK_EQ(rig.exporter.live(), 0u); }

/* A GPU whose only mappable type is cached answers every class with it, and
 * asks it once per request: one choice repeated is one choice. */
TEST(host_memory_source_choice, one_type_serving_every_preference_is_tried_once)
{ Rig rig;
  rig.exporter.add_type(local, true);
  rig.exporter.add_type(visible | coherent | cached, true);
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 1u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_BACK});
  const std::uint32_t before = rig.exporter.attempts();
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(2u, readback, memory), Status::no_memory);
  CHECK_EQ(rig.exporter.attempts(), before + 1u);
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(3u, upload, memory), Status::no_memory);
  CHECK_EQ(rig.exporter.attempts(), before + 2u);
  CHECK_EQ(memory.fd, -1);
  CHECK_EQ(rig.source.live(), 1u);
  rig.source.free(domain, 1u); }

/* A Vulkan request names a type of the guest's view and has no other: one the
 * source backs is exactly the host type behind it, refused with no fallback
 * when it has no room, and one the source does not back - or past the view -
 * is no request this GPU answers. */
TEST(host_memory_source_choice, a_vulkan_request_is_the_host_type_behind_its_guest_type_or_nothing)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, vulkan(Rig::guest_cached), memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 2u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_BACK});
  CHECK_STATUS(rig.allocate(2u, vulkan(Rig::guest_bar), memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 3u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  /* The caching is the chosen host type's own, as the GPU's exporter reports
   * it. */
  av::MemoryExporter              &exporter = rig.exporter;
  VkPhysicalDeviceMemoryProperties gpu = {};
  exporter.memory_properties(gpu);
  CHECK_EQ(gpu.memoryTypes[3].propertyFlags & cached, 0u);
  CHECK((gpu.memoryTypes[2].propertyFlags & cached) != 0u);
  const std::uint32_t before = rig.exporter.attempts();
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(3u, vulkan(Rig::guest_uncached), memory), Status::no_memory);
  CHECK_EQ(rig.exporter.attempts(), before + 1u);
  CHECK_EQ(rig.exporter.last_type(), 1u);
  /* The guest's 0, 1 and 2 are the host's 4, 5 and 0, none of them mapped;
   * the guest has six types. */
  constexpr std::uint32_t unmapped[] = {0u, 1u, 2u, 6u, 31u};
  for (const std::uint32_t type : unmapped) CHECK_STATUS(rig.allocate(4u, vulkan(type), memory), Status::invalid_parameter);
  CHECK_EQ(rig.exporter.attempts(), before + 1u);
  CHECK_EQ(rig.source.live(), 2u);
  rig.source.free_domain(domain); }

/* An integrated GPU's memory is all host visible: the guest's first type is
 * device-local memory the host allocates itself, never the source's, and the
 * two it backs follow, each its own host type. */
TEST(host_memory_source_choice, a_vulkan_request_for_device_local_memory_the_guest_never_maps_is_refused)
{ Rig rig;
  rig.exporter.add_type(local | visible | coherent, true);
  rig.exporter.add_type(local | visible | cached | coherent, true);
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, vulkan(0u), memory), Status::invalid_parameter);
  CHECK_EQ(rig.exporter.attempts(), 0u);
  CHECK_STATUS(rig.allocate(2u, vulkan(1u), memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 0u);
  CHECK_STATUS(rig.allocate(3u, vulkan(2u), memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 1u);
  rig.source.free_domain(domain);
  CHECK_EQ(rig.exporter.live(), 0u); }

/* The guest's view is the source's: what it maps is host visible, in the
 * spec's order, on the host's heaps. A GPU with no types yet - no compositing
 * context - has no view to give, and a type that appears later is in the
 * next one. */
TEST(host_memory_source_types, the_vulkan_view_is_what_the_source_maps_and_follows_new_types)
{ Rig rig;
  av::GuestMemoryTypes view;
  CHECK(!rig.source.vulkan_types(view));
  rig.exporter.add_heap(VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, 1u << 20);
  rig.exporter.add_heap(0u, 2u << 20);
  rig.exporter.add_type(local | visible | coherent, true, 0u);
  CHECK(rig.source.vulkan_types(view));
  VkPhysicalDeviceMemoryProperties seen = {};
  view.properties(seen);
  CHECK_EQ(seen.memoryTypeCount, 2u);
  CHECK_EQ(seen.memoryTypes[0].propertyFlags, local);
  CHECK_EQ(seen.memoryTypes[1].propertyFlags, local | visible | coherent);
  CHECK_EQ(seen.memoryHeapCount, 2u);
  CHECK_EQ(seen.memoryHeaps[1].size, VkDeviceSize{2u << 20});
  CHECK_EQ(view.source_type(1u), 0u);
  rig.exporter.add_type(visible | coherent, false, 1u);
  CHECK(rig.source.vulkan_types(view));
  view.properties(seen);
  CHECK_EQ(seen.memoryTypeCount, 3u);
  CHECK_EQ(seen.memoryTypes[0].propertyFlags, 0u);
  CHECK_EQ(seen.memoryTypes[0].heapIndex, 1u);
  CHECK_EQ(view.host_type(0u), 1u);
  CHECK(rig.source.invariants_hold()); }

/* A GPU with nothing mappable answers no class at all. */
TEST(host_memory_source_choice, a_gpu_with_nothing_mappable_answers_no_request)
{ Rig rig;
  rig.exporter.add_type(local, true);
  rig.exporter.add_type(visible | coherent, false);
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::invalid_parameter);
  CHECK_STATUS(rig.allocate(1u, readback, memory), Status::invalid_parameter);
  CHECK_EQ(rig.exporter.attempts(), 0u);
  CHECK_EQ(memory.fd, -1); }

/* What the guest sent is judged before the GPU is asked anything: no request,
 * a kind nobody defined, a type past any GPU's, no pages or part of one. */
TEST(host_memory_source_allocate, a_request_or_size_no_blob_can_have_is_refused_before_the_gpu_is_asked)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  const HostMemoryRequest garbage[] = {HostMemoryRequest(), HostMemoryRequest(4u), HostMemoryRequest(0x102u),
                                       HostMemoryRequest(0x2003u), HostMemoryRequest(0xFFFFFFFFu)};
  for (const HostMemoryRequest &request : garbage)
      CHECK_STATUS(rig.allocate(1u, request, memory), Status::invalid_parameter);
  CHECK_STATUS(rig.allocate(1u, upload, memory, 0u), Status::invalid_parameter);
  CHECK_STATUS(rig.allocate(1u, upload, memory, page + 1u), Status::invalid_parameter);
  CHECK_STATUS(rig.allocate(1u, upload, memory, page - 1u), Status::invalid_parameter);
  CHECK_EQ(memory.fd, -1);
  CHECK_EQ(rig.exporter.export_questions(1u), 0u);
  CHECK_EQ(rig.exporter.attempts(), 0u); }

/* One live allocation per key of a domain; the same key in another domain is
 * another device's. */
TEST(host_memory_source_allocate, a_key_already_live_in_its_domain_is_a_conflict)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(7u, upload, memory), Status::ok);
  const std::uint32_t before = rig.exporter.attempts();
  CHECK_STATUS(rig.allocate(7u, readback, memory), Status::conflict);
  CHECK_EQ(rig.exporter.attempts(), before);
  CHECK_STATUS(rig.allocate(7u, upload, memory, page, domain + 1u), Status::ok);
  CHECK_EQ(rig.source.live(), 2u);
  CHECK(rig.source.invariants_hold());
  rig.source.free_domain(domain);
  rig.source.free_domain(domain + 1u);
  CHECK_EQ(rig.exporter.live(), 0u); }

/* A driver may make more than was asked; the answer says what it made. */
TEST(host_memory_source_allocate, answers_the_size_the_gpu_made)
{ Rig rig;
  rig.discrete();
  rig.exporter.round_to(0x10000u);
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, readback, memory, 3u * page), Status::ok);
  CHECK_EQ(memory.size, 0x10000u);
  CHECK_EQ(rig.exporter.last_size(), 3u * page);
  rig.source.free(domain, 1u); }

/* The descriptor answered is the caller's own, of the same memory: closing it
 * leaves the source's, and a byte written through it is what the source
 * maps. */
TEST(host_memory_source_allocate, the_descriptor_answered_is_the_caller_s_own_of_the_same_memory)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(rig.source.allocate(domain, 1u, page, readback, memory), Status::ok);
  CHECK(memory.fd >= 0);
  auto *const view = static_cast<std::uint8_t *>(::mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_SHARED, memory.fd, 0));
  CHECK(view != MAP_FAILED);
  view[17] = 0x6Bu;
  SourceMemory *const held = rig.source.acquire(domain, 1u);
  CHECK(held != nullptr);
  CHECK_EQ(held->pointer()[17], 0x6Bu);
  CHECK(held->exported().fd() != memory.fd);
  ::munmap(view, page);
  ::close(memory.fd);
  CHECK(::fcntl(held->exported().fd(), F_GETFD) != -1);
  rig.source.release(*held);
  rig.source.free(domain, 1u);
  CHECK_EQ(rig.exporter.live(), 0u); }

/* Memory that cannot be shared is no memory the device can map: it goes
 * straight back to the driver. */
TEST(host_memory_source_allocate, memory_that_cannot_be_shared_goes_back_at_once)
{ Rig rig;
  rig.discrete();
  rig.exporter.fail_share_at(1u);
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::no_memory);
  CHECK_EQ(memory.fd, -1);
  CHECK_EQ(rig.exporter.live(), 0u);
  CHECK_EQ(rig.source.live(), 0u);
  CHECK(rig.source.acquire(domain, 1u) == nullptr); }

/* No room to keep it is no memory either - and the descriptor already shared
 * for the caller is closed with it, not left open in the host process. */
TEST(host_memory_source_allocate, no_room_to_keep_it_frees_the_memory_and_its_shared_descriptor)
{ Rig rig;
  rig.discrete();
  AvHostMemory warm = {};
  CHECK_STATUS(rig.allocate(1u, upload, warm), Status::ok);
  rig.source.free(domain, 1u);
  const std::uint32_t descriptors = open_descriptors();
  rig.platform.fail_allocation_at(1u);
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(rig.source.allocate(domain, 2u, page, upload, memory), Status::no_memory);
  CHECK_EQ(memory.fd, -1);
  CHECK_EQ(rig.exporter.live(), 0u);
  CHECK_EQ(rig.source.live(), 0u);
  CHECK_EQ(open_descriptors(), descriptors);
  CHECK(rig.source.invariants_hold()); }

/* The device's hold and an import's are counted alike: memory the device let
 * go of lives while an import holds it, answers to its key no longer - the
 * guest may reuse the key at once - and goes back to the driver with the
 * last release. */
TEST(host_memory_source_lifetime, an_import_keeps_memory_the_device_let_go_of)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, readback, memory), Status::ok);
  SourceMemory *const imported = rig.source.acquire(domain, 1u);
  CHECK(imported != nullptr);
  CHECK_EQ(imported->references(), 2u);
  CHECK(rig.source.invariants_hold());
  rig.source.free(domain, 1u);
  CHECK(rig.source.invariants_hold());
  CHECK_EQ(rig.exporter.live(), 1u);
  CHECK(rig.source.acquire(domain, 1u) == nullptr);
  /* The key is the guest's again, with memory of its own. */
  CHECK_STATUS(rig.allocate(1u, readback, memory), Status::ok);
  CHECK_EQ(rig.source.live(), 2u);
  CHECK(rig.source.invariants_hold());
  rig.source.release(*imported);
  CHECK_EQ(rig.source.live(), 1u);
  CHECK_EQ(rig.exporter.live(), 1u);
  rig.source.free(domain, 1u);
  CHECK_EQ(rig.exporter.live(), 0u);
  CHECK_EQ(rig.exporter.misuses(), 0u); }

/* Freeing a key nobody holds - never allocated, already freed, another
 * domain's - is nothing. */
TEST(host_memory_source_lifetime, freeing_a_key_not_live_is_nothing)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, readback, memory), Status::ok);
  rig.source.free(domain, 2u);
  rig.source.free(domain + 1u, 1u);
  CHECK_EQ(rig.source.live(), 1u);
  rig.source.free(domain, 1u);
  rig.source.free(domain, 1u);
  CHECK_EQ(rig.source.live(), 0u);
  CHECK_EQ(rig.exporter.frees(), 1u); }

/* A device going lets go of every key of its domain and no other's; what an
 * import still holds lives on, detached. */
TEST(host_memory_source_lifetime, freeing_a_domain_lets_go_of_its_keys_alone)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  for (std::uint64_t key = 1u; key <= 3u; key++) CHECK_STATUS(rig.allocate(key, upload, memory), Status::ok);
  CHECK_STATUS(rig.allocate(1u, upload, memory, page, domain + 1u), Status::ok);
  SourceMemory *const imported = rig.source.acquire(domain, 2u);
  CHECK(imported != nullptr);
  rig.source.free_domain(domain);
  CHECK(rig.source.invariants_hold());
  CHECK_EQ(rig.source.live(), 2u);
  CHECK(!imported->attached());
  CHECK(rig.source.acquire(domain, 1u) == nullptr);
  CHECK(rig.source.acquire(domain, 2u) == nullptr);
  SourceMemory *const other = rig.source.acquire(domain + 1u, 1u);
  CHECK(other != nullptr);
  rig.source.release(*other);
  rig.source.release(*imported);
  CHECK_EQ(rig.source.live(), 1u);
  rig.source.free_domain(domain + 1u);
  CHECK_EQ(rig.exporter.live(), 0u); }

/* The GPU going frees everything into its exporter, imported or not; the
 * exporter's own check fails the test on anything left. */
TEST(host_memory_source_lifetime, the_source_going_frees_everything_it_holds)
{ MockPlatform       platform;
  MockMemoryExporter exporter;
  exporter.add_type(visible | coherent, true);
  {
      HostMemorySource source(exporter, platform);
      AvHostMemory     memory = {-1, 0u, 0u};
      CHECK_STATUS(source.allocate(domain, 1u, page, upload, memory), Status::ok);
      ::close(memory.fd);
      CHECK_STATUS(source.allocate(domain, 2u, page, upload, memory), Status::ok);
      ::close(memory.fd);
      CHECK(source.acquire(domain, 2u) != nullptr);
      source.free(domain, 2u);
      CHECK_EQ(exporter.live(), 2u);
  }
  CHECK_EQ(exporter.live(), 0u);
  CHECK_EQ(exporter.misuses(), 0u); }

TEST(host_memory_source, reports_what_it_maps_and_holds)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, readback, memory, 2u * page), Status::ok);
  CHECK_STATUS(rig.allocate(2u, upload, memory), Status::ok);
  char        bytes[512] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  rig.source.report(writer);
  CHECK(std::strstr(writer.dump(), "memory_source") != nullptr);
  CHECK(std::strstr(writer.dump(), "mappable_types=0x000000000000000E") != nullptr);
  CHECK(std::strstr(writer.dump(), "visible_types=0x000000000000000E") != nullptr);
  CHECK(std::strstr(writer.dump(), "live=2") != nullptr);
  CHECK(std::strstr(writer.dump(), "bytes=12288") != nullptr);
  rig.source.free_domain(domain); }

/* Two live allocations answering one key would hand an import whichever the
 * pool found first. */
TEST(host_memory_source_invariants, two_allocations_answering_one_key_break_them)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  ExportedMemory second;
  CHECK_STATUS(rig.exporter.allocate(1u, page, second), Status::ok);
  CHECK_STATUS(HostMemorySourceProbe(rig.source).add(domain, 1u, second), Status::ok);
  CHECK(!rig.source.invariants_hold()); }

/* An allocation nobody holds is memory nothing will ever free. */
TEST(host_memory_source_invariants, an_allocation_nobody_references_breaks_them)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  SourceMemory *const held = rig.source.acquire(domain, 1u);
  CHECK(held != nullptr);
  HostMemorySourceProbe(rig.source).drain(*held);
  CHECK(!rig.source.invariants_hold()); }

/* A type taken as importable host-visible that the source does not map is a
 * type it would hand an upload while never having proved it exports. */
TEST(host_memory_source_invariants, a_visible_type_the_source_does_not_map_breaks_them)
{ Rig rig;
  rig.discrete();
  CHECK_EQ(rig.source.mappable_types(), 0xEu);
  CHECK(rig.source.invariants_hold());
  HostMemorySourceProbe(rig.source).mark_visible(0x1u);
  CHECK(!rig.source.invariants_hold()); }

/* The pool the allocations live in is the source's own: a pool that has lost
 * count of them is a source that has. */
TEST(host_memory_source_invariants, a_pool_that_breaks_its_own_breaks_them)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  HostMemorySourceProbe probe(rig.source);
  av::ObjectPoolProbe::shift_live_count(probe.pool(), 1);
  CHECK(!rig.source.invariants_hold());
  av::ObjectPoolProbe::shift_live_count(probe.pool(), -1);
  CHECK(rig.source.invariants_hold()); }

/* Every sequence a device and its imports drive keeps them, judged after
 * each step. */
TEST(host_memory_source_invariants, hold_across_a_sequence_of_allocations_imports_and_frees)
{ Rig rig;
  rig.discrete();
  AvHostMemory  memory = {};
  SourceMemory *imports[8] = {};
  std::uint32_t seed = 12345u;
  for (std::uint32_t step = 0u; step < 400u; step++) {
      seed = seed * 1103515245u + 12345u;
      const std::uint64_t key = (seed >> 8) % 8u + 1u;
      switch ((seed >> 4) % 4u) {
      case 0u: static_cast<void>(rig.allocate(key, (seed & 1u) != 0u ? upload : readback, memory)); break;
      case 1u: rig.source.free(domain, key); break;
      case 2u:
          if (imports[key - 1u] == nullptr) imports[key - 1u] = rig.source.acquire(domain, key);
          break;
      default:
          if (imports[key - 1u] != nullptr) rig.source.release(*imports[key - 1u]);
          imports[key - 1u] = nullptr;
          break;
      }
      CHECK(rig.source.invariants_hold());
  }
  for (SourceMemory *&held : imports)
      if (held != nullptr) rig.source.release(*held);
  rig.source.free_domain(domain);
  CHECK(rig.source.invariants_hold());
  CHECK_EQ(rig.source.live(), 0u);
  CHECK_EQ(rig.exporter.live(), 0u);
  CHECK_EQ(rig.exporter.misuses(), 0u); }
