#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "executor/exported_memory.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/host_ram_ledger.hpp"
#include "harness/executor/fake_memory_gauge.hpp"
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
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void shift_live_count(ObjectPool<T> &pool, std::int32_t by)
    {
        pool.live_ = static_cast<std::uint32_t>(static_cast<std::int32_t>(pool.live_) + by);
    }
};

}

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

class Rig {
public:
    Rig() : platform(), exporter(), source(exporter, platform) {}

    std::uint32_t guest_type(std::uint32_t host_type, VkMemoryPropertyFlags flags)
    {
        av::GuestMemoryTypes types;
        CHECK(source.vulkan_types(types));
        VkPhysicalDeviceMemoryProperties shown = {};
        types.properties(shown);
        for (std::uint32_t type = 0u; type < shown.memoryTypeCount; type++)
            if (types.host_type(type) == host_type && shown.memoryTypes[type].propertyFlags == flags) return type;
        return av::GuestMemoryTypes::no_type;
    }
    std::uint32_t guest_uncached() { return guest_type(1u, visible | coherent); }
    std::uint32_t guest_cached() { return guest_type(2u, visible | coherent | cached); }
    std::uint32_t guest_bar() { return guest_type(3u, local | visible | coherent); }

    void discrete()
    {
        exporter.add_type(local, true);
        exporter.add_type(visible | coherent, true);
        exporter.add_type(visible | coherent | cached, true);
        exporter.add_type(local | visible | coherent, true);
        exporter.add_type(visible, true);
        exporter.add_type(visible | coherent, false);
    }

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

class DescriptorRoom {
public:
    DescriptorRoom() : saved_{}
    { ::getrlimit(RLIMIT_NOFILE, &saved_);
      int lowest_free = 0;
      while (::fcntl(lowest_free, F_GETFD) != -1) lowest_free++;
      rlimit limit = saved_;
      limit.rlim_cur = static_cast<rlim_t>(lowest_free) + 1u;
      ::setrlimit(RLIMIT_NOFILE, &limit); }
    ~DescriptorRoom() { ::setrlimit(RLIMIT_NOFILE, &saved_); }
    DescriptorRoom(const DescriptorRoom &) = delete;
    DescriptorRoom &operator=(const DescriptorRoom &) = delete;

private:
    rlimit saved_;
};

}

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

TEST(host_memory_source_types, a_type_appearing_later_is_asked_then_and_the_rest_are_not)
{ Rig rig;
  CHECK_EQ(rig.source.mappable_types(), 0u);
  rig.exporter.add_type(visible | coherent, true);
  CHECK_EQ(rig.source.mappable_types(), 0x1u);
  rig.exporter.add_type(visible | coherent | cached, true);
  CHECK_EQ(rig.source.mappable_types(), 0x3u);
  CHECK_EQ(rig.exporter.export_questions(0u), 1u);
  CHECK_EQ(rig.exporter.export_questions(1u), 1u); }

TEST(host_memory_source_types, never_asks_past_the_thirty_two_a_request_can_name)
{ Rig rig;
  for (std::uint32_t type = 0u; type < MockMemoryExporter::max_types; type++)
      rig.exporter.add_type(visible | coherent, type % 2u == 0u);
  rig.exporter.claim_types(40u);
  CHECK_EQ(rig.source.mappable_types(), 0x55555555u);
  CHECK_EQ(rig.exporter.misuses(), 0u); }

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

TEST(host_memory_source_choice, upload_falls_back_to_uncached_then_to_any_type)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 1u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  CHECK_EQ(rig.exporter.attempts(), 2u);

  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(2u, upload, memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 1u);

  const std::uint32_t before = rig.exporter.attempts();
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(3u, upload, memory), Status::ok);
  CHECK_EQ(rig.exporter.attempts(), before + 2u);
  CHECK_EQ(rig.exporter.last_type(), 1u);
  CHECK(rig.source.invariants_hold());
  rig.source.free_domain(domain);
  CHECK_EQ(rig.exporter.live(), 0u); }

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
  CHECK_STATUS(rig.allocate(4u, vulkan(rig.guest_bar()), memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 3u);
  CHECK(rig.source.invariants_hold());
  rig.source.free_domain(domain);
  CHECK_EQ(rig.exporter.live(), 0u); }

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

TEST(host_memory_source_choice, upload_refused_by_two_types_is_tried_on_a_third_the_preferences_did_not_name)
{ Rig rig;
  const std::uint32_t card = rig.exporter.add_heap(VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, 1u << 30);
  const std::uint32_t system = rig.exporter.add_heap(0u, 2u << 30);
  rig.exporter.add_type(visible | coherent, true, system);
  rig.exporter.add_type(visible | coherent | cached, true, system);
  rig.exporter.add_type(local | visible | coherent, true, card);
  AvHostMemory memory = {};
  CHECK_EQ(rig.source.mappable_types(), 0x7u);
  rig.exporter.fail_allocations(2u);
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  CHECK(rig.exporter.last_type() != 2u);
  rig.source.free_domain(domain);
  CHECK_EQ(rig.exporter.live(), 0u); }

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

TEST(host_memory_source_choice, a_vulkan_request_is_the_host_type_behind_its_guest_type_or_nothing)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, vulkan(rig.guest_cached()), memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 2u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_BACK});
  CHECK_STATUS(rig.allocate(2u, vulkan(rig.guest_bar()), memory), Status::ok);
  CHECK_EQ(rig.exporter.last_type(), 3u);
  CHECK_EQ(memory.caching, std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  av::MemoryExporter              &exporter = rig.exporter;
  VkPhysicalDeviceMemoryProperties gpu = {};
  exporter.memory_properties(gpu);
  CHECK_EQ(gpu.memoryTypes[3].propertyFlags & cached, 0u);
  CHECK((gpu.memoryTypes[2].propertyFlags & cached) != 0u);
  const std::uint32_t before = rig.exporter.attempts();
  rig.exporter.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(3u, vulkan(rig.guest_uncached()), memory), Status::no_memory);
  CHECK_EQ(rig.exporter.attempts(), before + 1u);
  CHECK_EQ(rig.exporter.last_type(), 1u);
  const std::uint32_t unmapped[] = {rig.guest_type(0u, local), rig.guest_type(4u, 0u), rig.guest_type(5u, 0u),
                                    32u, 31u};
  for (const std::uint32_t type : unmapped) CHECK_STATUS(rig.allocate(4u, vulkan(type), memory), Status::invalid_parameter);
  CHECK_EQ(rig.exporter.attempts(), before + 1u);
  CHECK_EQ(rig.source.live(), 2u);
  rig.source.free_domain(domain); }

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

TEST(host_memory_source_choice, a_gpu_with_nothing_mappable_answers_no_request)
{ Rig rig;
  rig.exporter.add_type(local, true);
  rig.exporter.add_type(visible | coherent, false);
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::invalid_parameter);
  CHECK_STATUS(rig.allocate(1u, readback, memory), Status::invalid_parameter);
  CHECK_EQ(rig.exporter.attempts(), 0u);
  CHECK_EQ(memory.fd, -1); }

TEST(host_memory_source_allocate, a_request_or_size_no_blob_can_have_is_refused_before_the_gpu_is_asked)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  const HostMemoryRequest garbage[] = {HostMemoryRequest(), HostMemoryRequest(6u), HostMemoryRequest(0x102u),
                                       HostMemoryRequest(0x2003u), HostMemoryRequest(0xFFFFFFFFu)};
  for (const HostMemoryRequest &request : garbage)
      CHECK_STATUS(rig.allocate(1u, request, memory), Status::invalid_parameter);
  CHECK_STATUS(rig.allocate(1u, upload, memory, 0u), Status::invalid_parameter);
  CHECK_STATUS(rig.allocate(1u, upload, memory, page + 1u), Status::invalid_parameter);
  CHECK_STATUS(rig.allocate(1u, upload, memory, page - 1u), Status::invalid_parameter);
  CHECK_EQ(memory.fd, -1);
  CHECK_EQ(rig.exporter.export_questions(1u), 0u);
  CHECK_EQ(rig.exporter.attempts(), 0u); }

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

TEST(host_memory_source_allocate, answers_the_size_the_gpu_made)
{ Rig rig;
  rig.discrete();
  rig.exporter.round_to(0x10000u);
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, readback, memory, 3u * page), Status::ok);
  CHECK_EQ(memory.size, 0x10000u);
  CHECK_EQ(rig.exporter.last_size(), 3u * page);
  rig.source.free(domain, 1u); }

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

TEST(host_memory_source_allocate, memory_that_cannot_be_shared_goes_back_at_once)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  {
    const DescriptorRoom room;
    CHECK_STATUS(rig.allocate(1u, upload, memory), Status::no_memory);
  }
  CHECK_EQ(memory.fd, -1);
  CHECK_EQ(rig.exporter.live(), 0u);
  CHECK_EQ(rig.source.live(), 0u);
  CHECK(rig.source.acquire(domain, 1u) == nullptr); }

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
  CHECK_STATUS(rig.allocate(1u, readback, memory), Status::ok);
  CHECK_EQ(rig.source.live(), 2u);
  CHECK(rig.source.invariants_hold());
  rig.source.release(*imported);
  CHECK_EQ(rig.source.live(), 1u);
  CHECK_EQ(rig.exporter.live(), 1u);
  rig.source.free(domain, 1u);
  CHECK_EQ(rig.exporter.live(), 0u);
  CHECK_EQ(rig.exporter.misuses(), 0u); }

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

TEST(host_memory_source, reports_the_card_the_cap_and_the_reserve_the_policy_made_of_it)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, readback, memory), Status::ok);
  rig.source.reserve_vram(600u);
  char        bytes[1024] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  rig.source.report(writer);
  CHECK(std::strstr(writer.dump(), "vram_total_bytes=1073741824") != nullptr);
  CHECK(std::strstr(writer.dump(), "vram_reserve_bytes=536870912") != nullptr);
  CHECK(std::strstr(writer.dump(), "vram_cap_bytes=536870912") != nullptr);
  rig.source.free_domain(domain); }

TEST(host_memory_source, a_report_while_a_worker_holds_the_pool_says_busy_and_walks_nothing)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, readback, memory), Status::ok);
  av::PosixLock &held = HostMemorySourceProbe(rig.source).lock();
  held.acquire();
  char            bytes[512] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  rig.source.report(writer);
  held.release();
  CHECK(std::strstr(writer.dump(), "busy=true") != nullptr);
  CHECK(std::strstr(writer.dump(), "live=") == nullptr);
  CHECK(std::strstr(writer.dump(), "bytes=") == nullptr);
  char            again[512] = {};
  av::StateWriter after(again, sizeof again);
  rig.source.report(after);
  CHECK(std::strstr(after.dump(), "busy=true") == nullptr);
  CHECK(std::strstr(after.dump(), "live=1") != nullptr);
  rig.source.free_domain(domain); }

TEST(host_memory_source_invariants, two_allocations_answering_one_key_break_them)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  ExportedMemory second;
  CHECK_STATUS(rig.exporter.allocate(1u, page, second), Status::ok);
  CHECK_STATUS(HostMemorySourceProbe(rig.source).add(domain, 1u, second), Status::ok);
  CHECK(!rig.source.invariants_hold()); }

TEST(host_memory_source_invariants, an_allocation_nobody_references_breaks_them)
{ Rig rig;
  rig.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(rig.allocate(1u, upload, memory), Status::ok);
  SourceMemory *const held = rig.source.acquire(domain, 1u);
  CHECK(held != nullptr);
  HostMemorySourceProbe(rig.source).drain(*held);
  CHECK(!rig.source.invariants_hold()); }

TEST(host_memory_source_invariants, a_visible_type_the_source_does_not_map_breaks_them)
{ Rig rig;
  rig.discrete();
  CHECK_EQ(rig.source.mappable_types(), 0xEu);
  CHECK(rig.source.invariants_hold());
  HostMemorySourceProbe(rig.source).mark_visible(0x1u);
  CHECK(!rig.source.invariants_hold()); }

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

TEST(host_memory_source_alias, a_key_of_another_source_names_the_same_memory_until_the_last_reference)
{ Rig lender;
  Rig joiner;
  lender.discrete();
  joiner.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(lender.allocate(1u, readback, memory), Status::ok);
  SourceMemory *const lent = lender.source.acquire(domain, 1u);
  CHECK(lent != nullptr);
  CHECK(joiner.source.acquire(2u, 5u) == nullptr);
  AvHostMemory described = {};
  CHECK_STATUS(joiner.source.alias(2u, 5u, lender.source, domain, 1u, described), Status::ok);
  CHECK_EQ(described.fd, -1);
  CHECK_EQ(described.size, page);
  CHECK_EQ(described.caching, lent->caching());
  CHECK_STATUS(joiner.source.alias(2u, 6u, lender.source, domain, 9u, described), Status::not_found);
  CHECK_STATUS(joiner.source.alias(2u, 5u, lender.source, domain, 1u, described), Status::conflict);
  SourceMemory *const seen = joiner.source.acquire(2u, 5u);
  CHECK(seen == lent);
  lender.source.release(*lent);
  lender.source.free(domain, 1u);
  CHECK_EQ(lender.source.live(), 1u);
  CHECK(joiner.source.acquire(2u, 5u) == seen);
  joiner.source.release(*seen);
  joiner.source.release(*seen);
  CHECK_EQ(lender.source.live(), 1u);
  joiner.source.free(2u, 5u);
  CHECK_EQ(lender.source.live(), 0u);
  CHECK_EQ(lender.exporter.live(), 0u);
  CHECK(joiner.source.invariants_hold());
  CHECK(lender.source.invariants_hold()); }

TEST(host_memory_source_alias, a_closed_domain_lets_go_of_its_aliases)
{ Rig lender;
  Rig joiner;
  lender.discrete();
  joiner.discrete();
  AvHostMemory memory = {};
  CHECK_STATUS(lender.allocate(1u, readback, memory), Status::ok);
  AvHostMemory described = {};
  CHECK_STATUS(joiner.source.alias(2u, 5u, lender.source, domain, 1u, described), Status::ok);
  lender.source.free(domain, 1u);
  CHECK_EQ(lender.source.live(), 1u);
  joiner.source.free_domain(2u);
  CHECK_EQ(lender.source.live(), 0u);
  CHECK(joiner.source.acquire(2u, 5u) == nullptr); }

namespace {

struct Hammer {
    HostMemorySource *source;
    std::uint32_t     domain;
    std::uint64_t     key;
};

void *hammer(void *argument)
{
    const Hammer *const work = static_cast<const Hammer *>(argument);
    for (std::uint32_t round = 0u; round < 20000u; round++) {
        SourceMemory *const held = work->source->acquire(work->domain, work->key);
        if (held != nullptr) work->source->release(*held);
    }
    return nullptr;
}

}

TEST(host_memory_source_alias, sources_aliasing_each_others_memory_are_taken_from_two_threads_at_once)
{ Rig first;
  Rig second;
  first.discrete();
  second.discrete();
  AvHostMemory memory = {};
  AvHostMemory described = {};
  CHECK_STATUS(first.allocate(1u, readback, memory, page, 1u), Status::ok);
  CHECK_STATUS(second.allocate(1u, readback, memory, page, 2u), Status::ok);
  CHECK_STATUS(second.source.alias(2u, 7u, first.source, 1u, 1u, described), Status::ok);
  CHECK_STATUS(first.source.alias(1u, 7u, second.source, 2u, 1u, described), Status::ok);
  Hammer one = {&second.source, 2u, 7u};
  Hammer two = {&first.source, 1u, 7u};
  pthread_t a;
  pthread_t b;
  CHECK_EQ(pthread_create(&a, nullptr, &hammer, &one), 0);
  CHECK_EQ(pthread_create(&b, nullptr, &hammer, &two), 0);
  pthread_join(a, nullptr);
  pthread_join(b, nullptr);
  first.source.release_aliases();
  second.source.release_aliases();
  first.source.free(1u, 1u);
  second.source.free(2u, 1u);
  CHECK_EQ(first.source.live(), 0u);
  CHECK_EQ(second.source.live(), 0u);
  CHECK(first.source.invariants_hold() && second.source.invariants_hold()); }

namespace {

class HostRamRig {
public:
    HostRamRig() : platform(), exporter(), source(exporter, platform), ledger()
    {
        const std::uint32_t card = exporter.add_heap(VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, 16ull << 30);
        const std::uint32_t system = exporter.add_heap(0u, 32ull << 30);
        card_ = card;
        exporter.add_type(visible | coherent, true, system);
        exporter.add_type(visible | coherent | cached, true, system);
        source.use_host_ram(&ledger);
    }

    Status allocate(std::uint64_t key, HostMemoryRequest request, std::uint64_t size)
    {
        AvHostMemory memory = {-1, 0u, 0u};
        const Status status = source.allocate(domain, key, size, request, memory);
        if (memory.fd >= 0) ::close(memory.fd);
        return status;
    }

    void add_bar() { exporter.add_type(local | visible | coherent, true, card_); }

    MockPlatform       platform;
    MockMemoryExporter exporter;
    HostMemorySource   source;
    av::HostRamLedger  ledger;

private:
    std::uint32_t card_ = 0u;
};

}

TEST(host_memory_source_host_ram, a_readback_in_system_memory_past_the_budget_is_refused_before_the_exporter_is_asked)
{ HostRamRig rig;
  rig.ledger.budget(3u * page);
  CHECK_STATUS(rig.allocate(1u, readback, 2u * page), Status::ok);
  const std::uint32_t asked = rig.exporter.attempts();
  CHECK_STATUS(rig.allocate(2u, readback, 2u * page), Status::no_memory);
  CHECK_EQ(rig.exporter.attempts(), asked);
  CHECK_EQ(rig.ledger.used(), 2u * page);
  CHECK(rig.ledger.refusals() != 0u);
  CHECK_STATUS(rig.allocate(3u, readback, page), Status::ok);
  CHECK_EQ(rig.source.host_ram_held(), 3u * page);
  rig.source.free_domain(domain); }

TEST(host_memory_source_host_ram, freeing_one_memory_gives_its_bytes_back_and_makes_room)
{ HostRamRig rig;
  rig.ledger.budget(2u * page);
  CHECK_STATUS(rig.allocate(1u, readback, 2u * page), Status::ok);
  CHECK_STATUS(rig.allocate(2u, readback, page), Status::no_memory);
  rig.source.free(domain, 1u);
  CHECK_EQ(rig.ledger.used(), 0u);
  CHECK_STATUS(rig.allocate(2u, readback, 2u * page), Status::ok);
  rig.source.free_domain(domain);
  CHECK_EQ(rig.ledger.used(), 0u); }

TEST(host_memory_source_host_ram, closing_a_domain_returns_everything_it_held)
{ HostRamRig rig;
  rig.ledger.budget(10u * page);
  CHECK_STATUS(rig.allocate(1u, readback, 3u * page), Status::ok);
  CHECK_STATUS(rig.allocate(2u, readback, 4u * page), Status::ok);
  CHECK_EQ(rig.ledger.used(), 7u * page);
  rig.source.free_domain(domain);
  CHECK_EQ(rig.ledger.used(), 0u);
  CHECK_EQ(rig.source.host_ram_held(), 0u); }

TEST(host_memory_source_host_ram, memory_a_session_still_holds_stays_charged_until_it_lets_go)
{ HostRamRig rig;
  rig.ledger.budget(10u * page);
  CHECK_STATUS(rig.allocate(1u, readback, 3u * page), Status::ok);
  SourceMemory *const lent = rig.source.acquire(domain, 1u);
  CHECK(lent != nullptr);
  rig.source.free(domain, 1u);
  CHECK_EQ(rig.ledger.used(), 3u * page);
  rig.source.release(*lent);
  CHECK_EQ(rig.ledger.used(), 0u); }

TEST(host_memory_source_host_ram, an_exporter_that_fails_gives_the_reservation_back)
{ HostRamRig rig;
  rig.ledger.budget(10u * page);
  rig.exporter.fail_allocations(4u);
  CHECK_STATUS(rig.allocate(1u, readback, page), Status::no_memory);
  CHECK_EQ(rig.ledger.used(), 0u); }

TEST(host_memory_source_host_ram, memory_in_the_cards_own_heap_is_not_charged_to_host_ram)
{ HostRamRig rig;
  rig.add_bar();
  rig.ledger.budget(page);
  CHECK_STATUS(rig.allocate(1u, upload, 8u * page), Status::ok);
  CHECK_EQ(rig.ledger.used(), 0u);
  rig.source.free_domain(domain); }

TEST(host_memory_source_host_ram, destroying_the_source_returns_what_was_never_freed)
{ MockPlatform       platform;
  MockMemoryExporter exporter;
  av::HostRamLedger  ledger;
  exporter.add_heap(0u, 32ull << 30);
  exporter.add_type(visible | coherent | cached, true, 0u);
  {
    HostMemorySource source(exporter, platform);
    source.use_host_ram(&ledger);
    AvHostMemory memory = {-1, 0u, 0u};
    CHECK_STATUS(source.allocate(domain, 1u, 5u * page, readback, memory), Status::ok);
    if (memory.fd >= 0) ::close(memory.fd);
    CHECK_EQ(ledger.used(), 5u * page);
  }
  CHECK_EQ(ledger.used(), 0u); }

TEST(host_memory_source_host_ram, binding_the_same_ledger_again_keeps_the_books_and_the_report_says_what_is_held)
{ HostRamRig rig;
  rig.ledger.budget(10u * page);
  CHECK_STATUS(rig.allocate(1u, readback, 3u * page), Status::ok);
  rig.source.use_host_ram(&rig.ledger);
  CHECK_EQ(rig.ledger.used(), 3u * page);
  char            bytes[1024] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  rig.source.report(writer);
  CHECK(std::strstr(writer.dump(), "host_ram_held_bytes=12288") != nullptr);
  rig.source.free_domain(domain);
  CHECK_EQ(rig.ledger.used(), 0u); }

TEST(host_memory_source_host_ram, no_room_to_keep_the_memory_gives_its_reservation_back)
{ HostRamRig rig;
  rig.ledger.budget(10u * page);
  CHECK_STATUS(rig.allocate(1u, readback, page), Status::ok);
  rig.source.free(domain, 1u);
  rig.platform.fail_allocation_at(1u);
  CHECK_STATUS(rig.allocate(2u, readback, 3u * page), Status::no_memory);
  CHECK_EQ(rig.ledger.used(), 0u);
  CHECK_EQ(rig.exporter.live(), 0u); }

TEST(host_memory_source_host_ram, memory_that_cannot_be_shared_gives_its_reservation_back)
{ HostRamRig rig;
  rig.ledger.budget(10u * page);
  {
    const DescriptorRoom room;
    CHECK_STATUS(rig.allocate(1u, readback, 3u * page), Status::no_memory);
  }
  CHECK_EQ(rig.ledger.used(), 0u);
  CHECK_EQ(rig.exporter.live(), 0u); }

TEST(host_memory_source_host_ram, system_memory_is_refused_while_the_host_is_below_its_floor_before_the_exporter_is_asked_and_allowed_when_it_recovers)
{ HostRamRig rig;
  av::FakeMemoryGauge gauge;
  gauge.free_bytes = 5u * page;
  rig.ledger.watch(&gauge);
  rig.ledger.floor(4u * page);
  CHECK_STATUS(rig.allocate(1u, readback, page), Status::ok);
  const std::uint32_t asked = rig.exporter.attempts();
  CHECK_STATUS(rig.allocate(2u, readback, 4u * page), Status::no_memory);
  CHECK_EQ(rig.exporter.attempts(), asked);
  gauge.set(64u * page, av::HostRamLedger::refresh_ms);
  CHECK_STATUS(rig.allocate(3u, readback, 4u * page), Status::ok);
  rig.source.free_domain(domain); }

TEST(host_memory_source_host_ram, memory_on_the_card_is_not_refused_for_host_ram_while_the_host_is_tight)
{ HostRamRig rig;
  rig.add_bar();
  av::FakeMemoryGauge gauge;
  gauge.free_bytes = 0u;
  rig.ledger.watch(&gauge);
  rig.ledger.floor(4u * page);
  CHECK_STATUS(rig.allocate(1u, readback, page), Status::no_memory);
  rig.source.free_domain(domain); }

namespace {

std::uint8_t first_byte_of(const AvHostMemory &memory)
{
    void *const view = ::mmap(nullptr, page, PROT_READ, MAP_SHARED, memory.fd, 0);
    if (view == MAP_FAILED) return 0x5Au;
    const std::uint8_t byte = *static_cast<const std::uint8_t *>(view);
    ::munmap(view, page);
    return byte;
}

}

TEST(host_memory_source_reuse, device_memory_another_domain_freed_is_zeroed_for_the_next_domain)
{ HostRamRig rig;
  rig.add_bar();
  rig.exporter.fill_new(0xABu);
  AvHostMemory first{-1, 0u, 0u};
  CHECK_STATUS(rig.source.allocate(1u, 1u, page, upload, first), Status::ok);
  CHECK_EQ(first_byte_of(first), 0xABu);
  ::close(first.fd);
  rig.source.free(1u, 1u);
  AvHostMemory second{-1, 0u, 0u};
  CHECK_STATUS(rig.source.allocate(2u, 1u, page, upload, second), Status::ok);
  CHECK_EQ(first_byte_of(second), 0u);
  ::close(second.fd);
  rig.source.free(2u, 1u); }

TEST(host_memory_source_reuse, a_domain_that_only_reuses_its_own_device_memory_pays_no_clear)
{ HostRamRig rig;
  rig.add_bar();
  rig.exporter.fill_new(0xABu);
  AvHostMemory first{-1, 0u, 0u};
  CHECK_STATUS(rig.source.allocate(1u, 1u, page, upload, first), Status::ok);
  CHECK_EQ(first_byte_of(first), 0xABu);
  ::close(first.fd);
  rig.source.free(1u, 1u);
  AvHostMemory again{-1, 0u, 0u};
  CHECK_STATUS(rig.source.allocate(1u, 2u, page, upload, again), Status::ok);
  CHECK_EQ(first_byte_of(again), 0xABu);
  ::close(again.fd);
  rig.source.free(1u, 2u); }

TEST(host_memory_source_reuse, system_memory_is_never_cleared_because_the_kernel_hands_it_out_zeroed)
{ HostRamRig rig;
  rig.exporter.fill_new(0xABu);
  AvHostMemory first{-1, 0u, 0u};
  CHECK_STATUS(rig.source.allocate(1u, 1u, page, readback, first), Status::ok);
  ::close(first.fd);
  rig.source.free(1u, 1u);
  AvHostMemory second{-1, 0u, 0u};
  CHECK_STATUS(rig.source.allocate(2u, 1u, page, readback, second), Status::ok);
  CHECK_EQ(first_byte_of(second), 0xABu);
  ::close(second.fd);
  rig.source.free(2u, 1u); }

TEST(host_memory_source_placed, a_placed_allocation_is_taken_once_by_its_owner_and_the_type_and_pages_it_was_made_for)
{ Rig rig;
  int fds[2] = {-1, -1};
  CHECK_EQ(::pipe(fds), 0);
  CHECK_STATUS(rig.source.place(7u, 0x1000u, 3u, 2u * page - 5u, AV_MEMORY_WRITE_BACK, fds[0]), Status::ok);
  CHECK_EQ(rig.source.placed(), 1u);
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(rig.source.take_placed(8u, 3u, 2u * page, memory), Status::not_found);
  CHECK_STATUS(rig.source.take_placed(7u, 4u, 2u * page, memory), Status::not_found);
  CHECK_STATUS(rig.source.take_placed(7u, 3u, page, memory), Status::not_found);
  CHECK_STATUS(rig.source.take_placed(7u, 3u, 2u * page, memory), Status::ok);
  CHECK(memory.fd >= 0 && memory.fd != fds[0]);
  CHECK_EQ(memory.size, 2u * page);
  CHECK_EQ(memory.caching, static_cast<std::uint32_t>(AV_MEMORY_WRITE_BACK));
  CHECK_EQ(rig.source.placed(), 0u);
  CHECK_STATUS(rig.source.take_placed(7u, 3u, 2u * page, memory), Status::not_found);
  ::close(memory.fd);
  ::close(fds[0]);
  ::close(fds[1]); }

TEST(host_memory_source_placed, the_oldest_of_equal_allocations_is_taken_first_and_a_freed_one_is_never_taken)
{ Rig rig;
  const int first = ::open("/dev/null", O_RDONLY);
  const int second = ::open("/dev/null", O_RDONLY);
  CHECK_STATUS(rig.source.place(7u, 0x1000u, 3u, page, AV_MEMORY_WRITE_COMBINED, first), Status::ok);
  CHECK_STATUS(rig.source.place(7u, 0x2000u, 3u, page, AV_MEMORY_WRITE_BACK, second), Status::ok);
  rig.source.drop_placed(9u, 0x1000u);
  CHECK_EQ(rig.source.placed(), 2u);
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(rig.source.take_placed(7u, 3u, page, memory), Status::ok);
  CHECK_EQ(memory.caching, static_cast<std::uint32_t>(AV_MEMORY_WRITE_COMBINED));
  ::close(memory.fd);
  rig.source.drop_placed(7u, 0x2000u);
  CHECK_EQ(rig.source.placed(), 0u);
  CHECK_STATUS(rig.source.take_placed(7u, 3u, page, memory), Status::not_found);
  ::close(first);
  ::close(second); }

TEST(host_memory_source_placed, an_owner_keeps_a_bounded_number_and_gives_all_up_together_and_bad_arguments_are_refused)
{ Rig rig;
  const int fd = ::open("/dev/null", O_RDONLY);
  for (std::uint64_t handle = 1u; handle <= 40u; handle++)
      CHECK_STATUS(rig.source.place(7u, handle, 2u, page, AV_MEMORY_WRITE_COMBINED, fd), Status::ok);
  CHECK_EQ(rig.source.placed(), 16u);
  CHECK_STATUS(rig.source.place(8u, 1u, 2u, page, AV_MEMORY_WRITE_COMBINED, fd), Status::ok);
  rig.source.drop_placed_of(7u);
  CHECK_EQ(rig.source.placed(), 1u);
  CHECK_STATUS(rig.source.place(0u, 1u, 2u, page, AV_MEMORY_WRITE_COMBINED, fd), Status::invalid_parameter);
  CHECK_STATUS(rig.source.place(7u, 0u, 2u, page, AV_MEMORY_WRITE_COMBINED, fd), Status::invalid_parameter);
  CHECK_STATUS(rig.source.place(7u, 1u, 32u, page, AV_MEMORY_WRITE_COMBINED, fd), Status::invalid_parameter);
  CHECK_STATUS(rig.source.place(7u, 1u, 2u, 0u, AV_MEMORY_WRITE_COMBINED, fd), Status::invalid_parameter);
  CHECK_STATUS(rig.source.place(7u, 1u, 2u, page, AV_MEMORY_WRITE_COMBINED, -1), Status::invalid_parameter);
  CHECK_EQ(rig.source.placed(), 1u);
  rig.source.drop_placed_of(8u);
  ::close(fd); }

TEST(host_memory_source_placed, a_placed_heap_is_taken_as_a_heap_and_never_as_a_vulkan_type)
{ Rig rig;
  const int fd = ::open("/dev/null", O_RDONLY);
  CHECK_STATUS(rig.source.place_heap(7u, 0x40u, 3u * page, AV_MEMORY_WRITE_COMBINED, fd), Status::ok);
  CHECK_STATUS(rig.source.place_heap(0u, 0x40u, page, AV_MEMORY_WRITE_COMBINED, fd), Status::invalid_parameter);
  CHECK_STATUS(rig.source.place_heap(7u, 0u, page, AV_MEMORY_WRITE_COMBINED, fd), Status::invalid_parameter);
  CHECK_STATUS(rig.source.place_heap(7u, 0x41u, 0u, AV_MEMORY_WRITE_COMBINED, fd), Status::invalid_parameter);
  CHECK_STATUS(rig.source.place_heap(7u, 0x41u, page, AV_MEMORY_WRITE_COMBINED, -1), Status::invalid_parameter);
  CHECK_EQ(rig.source.placed(), 1u);
  AvHostMemory memory = {-1, 0u, 0u};
  CHECK_STATUS(rig.source.take_placed(7u, 0u, 3u * page, memory), Status::not_found);
  CHECK_STATUS(rig.source.take_placed(7u, HostMemorySource::heap_type, 2u * page, memory), Status::not_found);
  CHECK_STATUS(rig.source.take_placed(7u, HostMemorySource::heap_type, 3u * page, memory), Status::ok);
  CHECK(memory.fd >= 0 && memory.fd != fd);
  CHECK_EQ(memory.caching, static_cast<std::uint32_t>(AV_MEMORY_WRITE_COMBINED));
  CHECK_EQ(rig.source.placed(), 0u);
  CHECK_STATUS(rig.source.place(7u, 0x42u, HostMemorySource::heap_type, page, AV_MEMORY_WRITE_BACK, fd),
               Status::invalid_parameter);
  ::close(memory.fd);
  ::close(fd); }

TEST(host_memory_source_placed, a_destroyed_heap_s_placement_is_dropped_by_its_handle)
{ Rig rig;
  const int fd = ::open("/dev/null", O_RDONLY);
  CHECK_STATUS(rig.source.place_heap(7u, 0x40u, page, AV_MEMORY_WRITE_BACK, fd), Status::ok);
  rig.source.drop_placed(7u, 0x40u);
  CHECK_EQ(rig.source.placed(), 0u);
  ::close(fd); }

TEST(host_memory_source_imports_heaps, a_driver_that_lists_no_type_like_the_exported_one_is_not_asked_to_import)
{ Rig rig;
  const std::uint32_t plain = rig.exporter.add_type(local, true);
  const std::uint32_t host = rig.exporter.add_type(visible | coherent, true);
  const std::uint32_t twin = rig.exporter.add_type(visible | coherent | cached, true);
  CHECK(plain != host && host != twin);
  CHECK(rig.source.imports_heaps());
  rig.exporter.refuse_alike(host);
  CHECK(rig.source.imports_heaps());
  Rig refused;
  refused.exporter.add_type(local, true);
  refused.exporter.add_type(visible | coherent, true);
  refused.exporter.refuse_alike(1u);
  CHECK(!refused.source.imports_heaps()); }

TEST(host_memory_source_imports_heaps, a_source_with_nothing_to_map_imports_nothing)
{ Rig rig;
  rig.exporter.add_type(local, true);
  CHECK(!rig.source.imports_heaps()); }
