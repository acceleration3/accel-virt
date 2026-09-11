#include "core/support/status.hpp"
#include "device/memory/host_memory_block.hpp"
#include "device/memory/hostmem_space.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

using av::HostMemoryBlock;
using av::HostmemSpace;
using av::MockHost;
using av::Status;

TEST(host_memory_block_create, holds_a_page_multiple_until_released)
{ MockHost host;
  HostMemoryBlock block(host);
  CHECK(!block.created());
  CHECK_STATUS(block.create(0x3000u), Status::ok);
  CHECK(block.created());
  CHECK_EQ(block.size(), 0x3000u);
  CHECK(block.handle() != nullptr);
  CHECK_EQ(host.live_host_memory(), 1u);
  CHECK_STATUS(block.create(0x1000u), Status::invalid_state);
  block.release();
  CHECK(!block.created());
  CHECK_EQ(host.live_host_memory(), 0u);
  block.release(); }

TEST(host_memory_block_create, an_empty_or_partial_page_size_or_no_host_memory_is_refused)
{ MockHost host;
  HostMemoryBlock block(host);
  CHECK_STATUS(block.create(0u), Status::invalid_parameter);
  CHECK_STATUS(block.create(0x1001u), Status::invalid_parameter);
  host.fail_host_memory_at(1u);
  CHECK_STATUS(block.create(0x1000u), Status::no_memory);
  CHECK(!block.created()); }

TEST(hostmem_space_admits, a_page_aligned_placement_inside_the_region)
{ MockHost host;
  host.set_hostmem_size(0x10000u);
  const HostmemSpace space(host);
  CHECK(space.admits(0u, 0x10000u));
  CHECK(space.admits(0xF000u, 0x1000u));
  CHECK(!space.admits(0xF000u, 0x2000u));
  CHECK(!space.admits(0x800u, 0x1000u));
  CHECK(!space.admits(0x1000u, 0u));
  CHECK(!space.admits(0x11000u, 0x1000u));
  CHECK(!space.admits(0x1000u, 0xFFFFFFFFFFFFF000ull)); }
