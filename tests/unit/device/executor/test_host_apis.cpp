/* The seam between the executor library and this host's own loaders. What is
 * asserted here is the contract every implementation of `HostApis` owes -
 * including the mock, which is what makes a host with no GPU, a host with two,
 * and a host whose OpenCL cannot import testable on a machine that is none of
 * those. */
#include "device/abi/av_executor.h"
#include "device/executor/host_apis.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::HostApis;
using av::MockHostApis;

namespace {

constexpr std::uint8_t intel[16] = {0x86, 0x80, 0xa6, 0x46, 0x0c, 0, 0, 0, 0, 0x02, 0, 0, 0, 0, 0, 0};

}  // namespace

/* A UUID is 16 bytes and the caller owns them: HostApis::gpu_uuid fills what it
 * was given and writes nothing else. */
TEST(host_apis, gpu_uuid_fills_the_sixteen_bytes_it_was_given)
{ MockHostApis mock;
  mock.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  HostApis &apis = mock;
  CHECK_EQ(apis.gpu_count(), 1u);

  std::uint8_t uuid[17];
  for (std::uint8_t &byte : uuid) byte = 0xCD;
  apis.gpu_uuid(0u, uuid);
  for (std::uint32_t i = 0u; i < 16u; i++) CHECK_EQ(uuid[i], intel[i]);
  /* The seventeenth byte was never theirs to write. */
  CHECK_EQ(uuid[16], std::uint8_t{0xCD}); }

/* An index nobody has, and a destination that is not there: neither is a
 * failure the library can report, so neither may touch anything. */
TEST(host_apis, an_index_nobody_has_leaves_the_destination_alone)
{ MockHostApis mock;
  mock.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  HostApis &apis = mock;

  std::uint8_t uuid[16];
  for (std::uint8_t &byte : uuid) byte = 0xCD;
  apis.gpu_uuid(1u, uuid);
  for (const std::uint8_t byte : uuid) CHECK_EQ(byte, std::uint8_t{0xCD});
  apis.gpu_uuid(0u, nullptr);

  CHECK_EQ(apis.sub_apis(1u), 0u);
  CHECK_EQ(apis.import_kinds(1u, AV_EXECUTOR_SUB_API_VULKAN), 0u); }

/* A sub-API value that is not one of the three the ABI defines imports
 * nothing, whatever the loader would have said. */
TEST(host_apis, an_api_the_abi_does_not_define_imports_nothing)
{ MockHostApis mock;
  mock.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, AV_BLOB_DMA_BUF,
               AV_BLOB_DMA_BUF);
  HostApis &apis = mock;
  CHECK_EQ(apis.import_kinds(0u, AV_EXECUTOR_SUB_API_VULKAN), std::uint32_t{AV_BLOB_HOST_POINTER});
  CHECK_EQ(apis.import_kinds(0u, 0u), 0u);
  CHECK_EQ(apis.import_kinds(0u, 8u), 0u); }

/* A host with no GPU at all answers zero rather than failing: the library still
 * loads and offers nothing. */
TEST(host_apis, a_host_with_no_gpu_answers_none)
{ MockHostApis mock;
  HostApis &apis = mock;
  CHECK_EQ(apis.gpu_count(), 0u);
  CHECK_EQ(apis.sub_apis(0u), 0u); }

/* HostApis::render_minor: -1 wherever the driver does not name a render node,
 * which is what the clock hold takes as "leave the clocks to the driver". The
 * mock's GPUs never name one, and an index nobody has cannot. */
TEST(host_apis, a_gpu_whose_driver_names_no_render_node_answers_minus_one)
{ MockHostApis mock;
  mock.add_gpu(intel, AV_EXECUTOR_SUB_API_VULKAN, AV_BLOB_HOST_POINTER, 0u, 0u);
  HostApis &apis = mock;
  CHECK_EQ(apis.render_minor(0u), -1);
  CHECK_EQ(apis.render_minor(1u), -1);
  CHECK_EQ(apis.render_minor(0xFFFFFFFFu), -1); }
