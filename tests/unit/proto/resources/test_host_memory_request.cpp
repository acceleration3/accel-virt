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
{ const std::uint32_t refused[] = {0u, 6u, 0xFFu, 0x100u, 0xFFFFFF06u};
  for (const std::uint32_t encoded : refused) CHECK(!HostMemoryRequest(encoded).valid()); }

TEST(host_memory_request, encodes_the_kind_low_and_the_vulkan_type_above_it)
{ CHECK_EQ(HostMemoryRequest(Kind::upload, 0u).encoded(), 0x001u);
  CHECK_EQ(HostMemoryRequest(Kind::readback, 0u).encoded(), 0x002u);
  CHECK_EQ(HostMemoryRequest(Kind::vulkan, 5u).encoded(), 0x503u);
  CHECK_EQ(HostMemoryRequest(Kind::vulkan, 31u).encoded(), 0x1F03u);
  CHECK(HostMemoryRequest(0x503u).kind() == Kind::vulkan);
  CHECK_EQ(HostMemoryRequest(0x503u).vulkan_type(), 5u);
  CHECK(HostMemoryRequest(0x001u).kind() == Kind::upload);
  CHECK_EQ(HostMemoryRequest(0x001u).vulkan_type(), 0u); }

TEST(host_memory_request, a_placed_request_names_a_type_like_a_vulkan_one_and_is_refused_past_the_last)
{ CHECK(HostMemoryRequest(Kind::placed, 0u).valid());
  CHECK(HostMemoryRequest(Kind::placed, HostMemoryRequest::memory_types - 1u).valid());
  CHECK(!HostMemoryRequest(Kind::placed, HostMemoryRequest::memory_types).valid());
  CHECK_EQ(HostMemoryRequest(Kind::placed, 5u).encoded(), 0x504u);
  CHECK(HostMemoryRequest(0x504u).kind() == Kind::placed);
  CHECK_EQ(HostMemoryRequest(0x504u).vulkan_type(), 5u); }

TEST(host_memory_request, a_placed_heap_request_names_no_type_and_both_placed_kinds_place)
{ CHECK(HostMemoryRequest(Kind::placed_heap, 0u).valid());
  CHECK(!HostMemoryRequest(Kind::placed_heap, 1u).valid());
  CHECK_EQ(HostMemoryRequest(Kind::placed_heap, 0u).encoded(), 0x005u);
  CHECK(HostMemoryRequest(0x005u).kind() == Kind::placed_heap);
  CHECK(HostMemoryRequest(Kind::placed_heap, 0u).places());
  CHECK(HostMemoryRequest(Kind::placed, 4u).places());
  CHECK(!HostMemoryRequest(Kind::vulkan, 4u).places());
  CHECK(!HostMemoryRequest(Kind::upload, 0u).places());
  CHECK(!HostMemoryRequest().places()); }
