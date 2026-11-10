#include "executor/owned_fd.hpp"
#include "executor/shim/vulkan_device_table.hpp"
#include "executor/shim/vulkan_memory_import.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <fcntl.h>
#include <unistd.h>

TEST(vulkan_memory_import, a_dma_buf_is_not_offered_to_a_type_the_device_does_not_list_and_stays_with_its_owner)
{ av::VulkanDeviceTable table;
  av::VulkanMemoryImport import(table);
  av::OwnedFd            descriptor(::open("/dev/null", O_RDONLY));
  CHECK(descriptor.held());
  std::uint64_t made = 0u;
  CHECK(!import.import_dmabuf(descriptor, 4096u, 0u, made));
  CHECK(!import.import_dmabuf(descriptor, 4096u, 31u, made));
  CHECK(descriptor.held());
  CHECK_EQ(made, std::uint64_t{0u});
  CHECK_EQ(import.held(), 0u); }
