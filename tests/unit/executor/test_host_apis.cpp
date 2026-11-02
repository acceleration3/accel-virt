#include "device/abi/av_executor.h"
#include "executor/host_apis.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::HostApis;
using av::MockHostApis;

namespace {

constexpr std::uint8_t intel[16] = {0x86, 0x80, 0xa6, 0x46, 0x0c, 0, 0, 0, 0, 0x02, 0, 0, 0, 0, 0, 0};

}

TEST(host_apis, gpu_uuid_fills_the_sixteen_bytes_it_was_given)
{ MockHostApis mock;
  mock.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  HostApis &apis = mock;
  CHECK_EQ(apis.gpu_count(), 1u);

  std::uint8_t uuid[17];
  for (std::uint8_t &byte : uuid) byte = 0xCD;
  apis.gpu_uuid(0u, uuid);
  for (std::uint32_t i = 0u; i < 16u; i++) CHECK_EQ(uuid[i], intel[i]);
  CHECK_EQ(uuid[16], std::uint8_t{0xCD}); }

TEST(host_apis, an_index_nobody_has_leaves_the_destination_alone)
{ MockHostApis mock;
  mock.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  HostApis &apis = mock;

  std::uint8_t uuid[16];
  for (std::uint8_t &byte : uuid) byte = 0xCD;
  apis.gpu_uuid(1u, uuid);
  for (const std::uint8_t byte : uuid) CHECK_EQ(byte, std::uint8_t{0xCD});
  apis.gpu_uuid(0u, nullptr);
  CHECK_EQ(apis.sub_apis(1u), 0u);
  CHECK(apis.memory_source(1u) == nullptr);
  CHECK(apis.shares(1u) == nullptr);
  CHECK(apis.surfaces(1u) == nullptr); }

TEST(host_apis, a_gpu_has_a_memory_source_unless_its_context_would_not_open)
{ MockHostApis mock;
  mock.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  HostApis &apis = mock;
  CHECK(apis.memory_source(0u) == &mock.source());
  mock.withhold_memory_source();
  CHECK(apis.memory_source(0u) == nullptr); }

TEST(host_apis, a_host_with_no_gpu_answers_none)
{ MockHostApis mock;
  HostApis &apis = mock;
  CHECK_EQ(apis.gpu_count(), 0u);
  CHECK_EQ(apis.sub_apis(0u), 0u);
  CHECK(apis.memory_source(0u) == nullptr); }

TEST(host_apis, a_gpu_whose_driver_names_no_render_node_answers_minus_one)
{ MockHostApis mock;
  mock.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN);
  HostApis &apis = mock;
  CHECK_EQ(apis.render_minor(0u), -1);
  CHECK_EQ(apis.render_minor(1u), -1);
  CHECK_EQ(apis.render_minor(0xFFFFFFFFu), -1); }
