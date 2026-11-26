#include "device/abi/av_executor.h"
#include "executor/executor_log.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/source_memory.hpp"
#include "executor/vulkan/guest_memory_types.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
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
constexpr std::uint64_t bytes = 64u * 4096u;

av::LoaderHostApis &opened_host()
{ static av::RecordingLog sink;
  static av::ExecutorLog  log(sink);
  static av::LoaderHostApis host(log);
  host.open();
  return host; }

bool driver_gpu(av::LoaderHostApis &host, std::uint32_t index, VkPhysicalDeviceProperties &described,
                VkPhysicalDeviceMemoryProperties &memory)
{ std::uint8_t uuid[16] = {};
  host.gpu_uuid(index, uuid);
  VkApplicationInfo application = {};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application.apiVersion = (1u << 22) | (1u << 12);
  VkInstanceCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create.pApplicationInfo = &application;
  VkInstance instance = VK_NULL_HANDLE;
  if (::vkCreateInstance(&create, nullptr, &instance) != VK_SUCCESS) return false;
  std::uint32_t    count = 8u;
  VkPhysicalDevice devices[8] = {};
  bool             found = false;
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
          described = properties.properties;
          ::vkGetPhysicalDeviceMemoryProperties(devices[device], &memory);
          found = true;
      }
  }
  ::vkDestroyInstance(instance, nullptr);
  return found; }

std::uint32_t driver_flags(av::LoaderHostApis &host, std::uint32_t index, std::uint32_t type)
{ VkPhysicalDeviceProperties       described = {};
  VkPhysicalDeviceMemoryProperties memory = {};
  if (!driver_gpu(host, index, described, memory)) return 0u;
  return type < memory.memoryTypeCount ? memory.memoryTypes[type].propertyFlags : 0u; }

bool imports_host_visible(av::LoaderHostApis &host, std::uint32_t index, int fd)
{ std::uint8_t uuid[16] = {};
  host.gpu_uuid(index, uuid);
  VkApplicationInfo application = {};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application.apiVersion = (1u << 22) | (1u << 12);
  VkInstanceCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create.pApplicationInfo = &application;
  VkInstance instance = VK_NULL_HANDLE;
  if (::vkCreateInstance(&create, nullptr, &instance) != VK_SUCCESS) return false;
  std::uint32_t    count = 8u;
  VkPhysicalDevice devices[8] = {};
  bool             visible = false;
  if (::vkEnumeratePhysicalDevices(instance, &count, devices) < VK_SUCCESS) count = 0u;
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
      const char *const extensions[] = {"VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf"};
      const float       priority = 1.0f;
      VkDeviceQueueCreateInfo queue = {};
      queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue.queueCount = 1u;
      queue.pQueuePriorities = &priority;
      VkDeviceCreateInfo made = {};
      made.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      made.queueCreateInfoCount = 1u;
      made.pQueueCreateInfos = &queue;
      made.enabledExtensionCount = 2u;
      made.ppEnabledExtensionNames = extensions;
      VkDevice importer = VK_NULL_HANDLE;
      if (::vkCreateDevice(devices[device], &made, nullptr, &importer) != VK_SUCCESS) continue;
      auto *const fd_properties = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
          ::vkGetDeviceProcAddr(importer, "vkGetMemoryFdPropertiesKHR"));
      VkMemoryFdPropertiesKHR answered = {};
      answered.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
      VkPhysicalDeviceMemoryProperties memory = {};
      ::vkGetPhysicalDeviceMemoryProperties(devices[device], &memory);
      if (fd_properties != nullptr &&
          fd_properties(importer, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd, &answered) == VK_SUCCESS)
          for (std::uint32_t type = 0u; type < memory.memoryTypeCount; type++)
              visible = visible || ((answered.memoryTypeBits >> type & 1u) != 0u &&
                                    (memory.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u);
      ::vkDestroyDevice(importer, nullptr);
  }
  ::vkDestroyInstance(instance, nullptr);
  return visible; }

std::uint8_t *map(const AvHostMemory &memory)
{ void *const mapped = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, memory.fd, 0);
  return mapped != MAP_FAILED ? static_cast<std::uint8_t *>(mapped) : nullptr; }

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

}

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

TEST(compositor_memory_device, upload_and_readback_import_back_as_memory_a_cpu_maps)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t index = 0u; index < host.gpu_count(); index++) {
      HostMemorySource *const source = host.memory_source(index);
      if (source == nullptr) continue;
      const HostMemoryRequest requests[] = {HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u),
                                            HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u)};
      std::uint64_t key = 300u;
      for (const HostMemoryRequest &request : requests) {
          AvHostMemory answered = {-1, 0u, 0u};
          CHECK_STATUS(source->allocate(domain, key, bytes, request, answered), Status::ok);
          CHECK(imports_host_visible(host, index, answered.fd));
          source->free(domain, key);
          if (answered.fd >= 0) ::close(answered.fd);
          key++;
      }
  } }

TEST(compositor_memory_device, every_guest_type_the_guest_maps_is_shared_memory_of_its_host_type_and_the_rest_are_refused)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t index = 0u; index < host.gpu_count(); index++) {
      HostMemorySource *const source = host.memory_source(index);
      if (source == nullptr) continue;
      CHECK(source->mappable_types() != 0u);
      av::GuestMemoryTypes types;
      CHECK(source->vulkan_types(types));
      VkPhysicalDeviceMemoryProperties shown = {};
      types.properties(shown);
      for (std::uint32_t guest = 0u; guest < shown.memoryTypeCount; guest++) {
          const std::uint32_t key = 100u + guest;
          AvHostMemory        answered = {-1, 0u, 0u};
          const Status        status = source->allocate(domain, key, bytes,
                                                        HostMemoryRequest(HostMemoryRequest::Kind::vulkan, guest), answered);
          if ((shown.memoryTypes[guest].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0u) {
              CHECK_STATUS(status, Status::invalid_parameter);
              CHECK_EQ(answered.fd, -1);
              continue;
          }
          CHECK_STATUS(status, Status::ok);
          const std::uint32_t type = types.host_type(guest);
          const std::uint32_t flags = driver_flags(host, index, type);
          CHECK((source->mappable_types() >> type & 1u) != 0u);
          CHECK((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u);
          CHECK((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0u);
          const std::uint32_t caching = (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0u ? AV_MEMORY_WRITE_BACK
                                                                                         : AV_MEMORY_WRITE_COMBINED;
          CHECK_EQ(answered.caching, caching);
          SourceMemory *const held = source->acquire(domain, key);
          CHECK(held != nullptr);
          if (held != nullptr) {
              CHECK_EQ(held->memory_type(), type);
              source->release(*held);
          }
          judge_shared(*source, key, answered);
          source->free(domain, key);
          ::close(answered.fd);
      }
      CHECK_EQ(source->live(), 0u);
      CHECK(source->invariants_hold());
  } }

TEST(compositor_memory_device, an_uncached_type_of_an_integrated_gpu_is_never_mapped_while_the_guest_pat_is_not_honoured)
{ av::LoaderHostApis &host = opened_host();
  for (std::uint32_t index = 0u; index < host.gpu_count(); index++) {
      HostMemorySource *const          source = host.memory_source(index);
      VkPhysicalDeviceProperties       described = {};
      VkPhysicalDeviceMemoryProperties memory = {};
      if (source == nullptr || !driver_gpu(host, index, described, memory)) continue;
      if (described.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) continue;
      const std::uint32_t mappable = source->mappable_types();
      for (std::uint32_t type = 0u; type < memory.memoryTypeCount && type < HostMemoryRequest::memory_types; type++)
          if ((mappable >> type & 1u) != 0u)
              CHECK((memory.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0u);
      CHECK(source->invariants_hold());
  } }

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
