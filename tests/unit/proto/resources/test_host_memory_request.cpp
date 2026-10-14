/* The kind of host memory a guest asks for. One encoding crosses three
 * boundaries, so what one side builds the next must read back unchanged, and
 * every value no driver builds must be refused the same way everywhere. */
#include "harness/framework/test_registry.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>

using av::HostMemoryRequest;
using Kind = av::HostMemoryRequest::Kind;

TEST(host_memory_request, each_kind_survives_its_encoding)
{ const HostMemoryRequest upload(Kind::upload, 0u);
  const HostMemoryRequest readback(Kind::readback, 0u);
  const HostMemoryRequest vulkan(Kind::vulkan, 31u);
  CHECK(HostMemoryRequest(upload.encoded()).valid());
  CHECK(HostMemoryRequest(upload.encoded()).kind() == Kind::upload);
  CHECK(HostMemoryRequest(readback.encoded()).kind() == Kind::readback);
  CHECK(HostMemoryRequest(vulkan.encoded()).kind() == Kind::vulkan);
  CHECK_EQ(HostMemoryRequest(vulkan.encoded()).vulkan_type(), 31u);
  CHECK_EQ(HostMemoryRequest(Kind::vulkan, 0u).vulkan_type(), 0u);
  CHECK(HostMemoryRequest(Kind::vulkan, 0u).valid()); }

TEST(host_memory_request, no_request_is_refused_so_an_unset_field_never_allocates)
{ CHECK(!HostMemoryRequest().valid());
  CHECK_EQ(HostMemoryRequest().encoded(), 0u); }

TEST(host_memory_request, a_vulkan_type_past_the_last_one_vulkan_has_is_refused)
{ CHECK(HostMemoryRequest(Kind::vulkan, HostMemoryRequest::memory_types - 1u).valid());
  CHECK(!HostMemoryRequest(Kind::vulkan, HostMemoryRequest::memory_types).valid());
  CHECK(!HostMemoryRequest(Kind::vulkan, 0xFFFFFFu).valid()); }

TEST(host_memory_request, a_class_that_names_a_type_is_refused_rather_than_read_as_vulkan)
{ CHECK(!HostMemoryRequest(Kind::upload, 1u).valid());
  CHECK(!HostMemoryRequest(Kind::readback, 3u).valid()); }

TEST(host_memory_request, a_kind_this_protocol_does_not_have_is_refused)
{ const std::uint32_t refused[] = {0u, 4u, 0xFFu, 0x100u, 0xFFFFFF04u};
  for (const std::uint32_t encoded : refused) CHECK(!HostMemoryRequest(encoded).valid()); }
