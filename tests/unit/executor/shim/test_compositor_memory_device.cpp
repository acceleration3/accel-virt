/* A host GPU's memory as its compositing context allocates it, on this
 * machine's own GPUs, through the memory source the executor gives every
 * guest-mapped byte from (docs/host-memory.md). What the device and the guest
 * rely on is judged on the real driver: every class allocates, the dma-buf
 * the device maps shows exactly what the source's own mapping holds and the
 * other way round, the caching answered is the memory type's, and only
 * host-visible coherent types are ever offered. A GPU whose compositing
 * context does not open, or a host with none, judges nothing and says so. */
#include "device/abi/av_executor.h"
#include "executor/host_memory_source.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/source_memory.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vulkan/vulkan.h>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libvulkan\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libnvidia\n"
           "leak:nvidia_icd\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libglapi\n"
           "leak:libz\n";
}

using av::HostMemoryRequest;
using av::HostMemorySource;
using av::SourceMemory;
using av::Status;

namespace {

constexpr std::uint32_t domain = 1u;
/* Large enough to span several pages of any driver's mapping granularity. */
constexpr std::uint64_t bytes = 64u * 4096u;

/* The host's loaders, opened once for the binary. */
av::LoaderHostApis &opened_host()
{ static av::LoaderHostApis host;
  host.open();
  return host; }

/* The flags the host's own enumeration answers for GPU `index`'s memory type
 * `type`, found by the GPU's UUID. */
std::uint32_t driver_flags(av::LoaderHostApis &host, std::uint32_t index, std::uint32_t type)
{ std::uint8_t uuid[16] = {};
  host.gpu_uuid(index, uuid);
  VkApplicationInfo application = {};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application.apiVersion = (1u << 22) | (1u << 12);
  VkInstanceCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create.pApplicationInfo = &application;
  VkInstance instance = VK_NULL_HANDLE;
  if (::vkCreateInstance(&create, nullptr, &instance) != VK_SUCCESS) return 0u;
  std::uint32_t    count = 8u;
  VkPhysicalDevice devices[8] = {};
  std::uint32_t    flags = 0u;
  if (::vkEnumeratePhysicalDevices(instance, &count, devices) >= VK_SUCCESS) {
      for (std::uint32_t device = 0u; device < count; device++) {
          VkPhysicalDeviceIDProperties id = {};
          id.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
          VkPhysicalDeviceProperties2 properties = {};
          properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
          properties.pNext = &id;
          ::vkGetPhysicalDeviceProperties2(devices[device], &properties);
          bool same = true;
          for (std::uint32_t byte = 0u; byte < 16u; byte++) same = same && id.deviceUUID[byte] == uuid[byte];
          if (!same) continue;
          VkPhysicalDeviceMemoryProperties memory = {};
          ::vkGetPhysicalDeviceMemoryProperties(devices[device], &memory);
          flags = type < memory.memoryTypeCount ? memory.memoryTypes[type].propertyFlags : 0u;
      }
  }
  ::vkDestroyInstance(instance, nullptr);
  return flags; }

/* What the device does with an answer: maps the dma-buf, here for reading
 * and writing; null where it cannot. */
std::uint8_t *map(const AvHostMemory &memory)
{ void *const mapped = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, memory.fd, 0);
  return mapped != MAP_FAILED ? static_cast<std::uint8_t *>(mapped) : nullptr; }

/* A write through either view is the other's: the guest's window and the
 * host API that imported the memory see one allocation. */
void judge_shared(HostMemorySource &source, std::uint64_t key, const AvHostMemory &answered)
{ std::uint8_t *const window = map(answered);
  CHECK(window != nullptr);
  SourceMemory *const held = source.acquire(domain, key);
  CHECK(held != nullptr);
  if (window == nullptr || held == nullptr) return;
  std::uint8_t *const own = held->pointer();
  CHECK(own != nullptr);
  for (std::uint64_t at = 0u; at < bytes; at += 4096u) window[at] = static_cast<std::uint8_t>(at / 4096u + 1u);
  window[bytes - 1u] = 0xC3u;
  for (std::uint64_t at = 0u; at < bytes; at += 4096u) CHECK_EQ(own[at], static_cast<std::uint8_t>(at / 4096u + 1u));
  CHECK_EQ(own[bytes - 1u], 0xC3u);
  own[123] = 0x5Au;
  own[bytes - 2u] = 0xA5u;
  CHECK_EQ(window[123], 0x5Au);
  CHECK_EQ(window[bytes - 2u], 0xA5u);
  source.release(*held);
  ::munmap(window, bytes); }

}  // namespace

/* Every class a guest asks for allocates on every GPU with a source, and is
 * one allocation both views share. */
TEST(compositor_memory_device, every_class_allocates_memory_both_views_share)
{ av::LoaderHostApis &host = opened_host();
  std::uint32_t judged = 0u;
  for (std::uint32_t index = 0u; index < host.gpu_count(); index++) {
      HostMemorySource *const source = host.memory_source(index);
      if (source == nullptr) continue;
      judged++;
      const HostMemoryRequest requests[] = {HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u),
                                            HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u)};
      std::uint64_t key = 1u;
      for (const HostMemoryRequest &request : requests) {
          AvHostMemory answered = {-1, 0u, 0u};
          CHECK_STATUS(source->allocate(domain, key, bytes, request, answered), Status::ok);
          CHECK(answered.fd >= 0);
          CHECK(answered.size >= bytes);
          judge_shared(*source, key, answered);
          source->free(domain, key);
          ::close(answered.fd);
          key++;
      }
      CHECK_EQ(source->live(), 0u);
  }
  if (judged == 0u) std::fprintf(stderr, "no GPU here has a memory source: nothing judged\n"); }

/* Each type the source maps is host visible and coherent on the driver, and
 * allocated as exactly itself, cached as the driver says it is: what the
 * guest is told to map with is what the host maps with. A Vulkan guest's
 * own allocation of the type is the same shared memory. */
TEST(compositor_memory_device, every_mappable_type_is_coherent_and_answers_its_own_caching)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t index = 0u; index < host.gpu_count(); index++) {
      HostMemorySource *const source = host.memory_source(index);
      if (source == nullptr) continue;
      const std::uint32_t mappable = source->mappable_types();
      CHECK(mappable != 0u);
      for (std::uint32_t type = 0u; type < HostMemoryRequest::memory_types; type++) {
          if ((mappable >> type & 1u) == 0u) continue;
          const std::uint32_t flags = driver_flags(host, index, type);
          CHECK((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u);
          CHECK((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0u);
          AvHostMemory answered = {-1, 0u, 0u};
          CHECK_STATUS(source->allocate(domain, 100u + type, bytes,
                                        HostMemoryRequest(HostMemoryRequest::Kind::vulkan, type), answered),
                       Status::ok);
          const std::uint32_t caching = (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0u ? AV_MEMORY_WRITE_BACK
                                                                                         : AV_MEMORY_WRITE_COMBINED;
          CHECK_EQ(answered.caching, caching);
          SourceMemory *const held = source->acquire(domain, 100u + type);
          CHECK(held != nullptr);
          if (held != nullptr) {
              CHECK_EQ(held->memory_type(), type);
              source->release(*held);
          }
          judge_shared(*source, 100u + type, answered);
          source->free(domain, 100u + type);
          ::close(answered.fd);
      }
      /* A type it does not map is no request this GPU answers. */
      for (std::uint32_t type = 0u; type < HostMemoryRequest::memory_types; type++) {
          if ((mappable >> type & 1u) != 0u) continue;
          AvHostMemory answered = {-1, 0u, 0u};
          CHECK_STATUS(source->allocate(domain, 200u, bytes, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, type),
                                        answered),
                       Status::invalid_parameter);
          CHECK_EQ(answered.fd, -1);
      }
      CHECK(source->invariants_hold());
  } }

/* The memory outlives the device's hold while an import holds it, and is the
 * driver's again once nothing does: an import's view keeps reading what was
 * written after the device let go. */
TEST(compositor_memory_device, an_import_keeps_the_memory_after_the_device_lets_go)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t index = 0u; index < host.gpu_count(); index++) {
      HostMemorySource *const source = host.memory_source(index);
      if (source == nullptr) continue;
      AvHostMemory answered = {-1, 0u, 0u};
      CHECK_STATUS(source->allocate(domain, 7u, bytes, HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u),
                                    answered),
                   Status::ok);
      SourceMemory *const imported = source->acquire(domain, 7u);
      CHECK(imported != nullptr);
      if (imported == nullptr) continue;
      source->free(domain, 7u);
      CHECK_EQ(source->live(), 1u);
      std::uint8_t *const window = map(answered);
      CHECK(window != nullptr);
      if (window != nullptr) {
          window[4096u] = 0x42u;
          CHECK_EQ(imported->pointer()[4096u], 0x42u);
          ::munmap(window, bytes);
      }
      source->release(*imported);
      CHECK_EQ(source->live(), 0u);
      ::close(answered.fd);
  } }
