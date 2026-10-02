#include "core/support/status.hpp"
#include "miniport/virtio/dma_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

struct DmaBufferProbe {
    explicit DmaBufferProbe(DmaBuffer &target) : buffer(target) {}
    void set_size(std::size_t size) { buffer.size_ = size; }
    void set_physical_address(std::uint64_t address) { buffer.physical_address_ = address; }
    DmaBuffer &buffer;
};

}  // namespace av

using av::DmaBuffer;
using av::DmaBufferProbe;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

TEST(dma_buffer, starts_empty)
{ MockPlatform platform; const DmaBuffer buffer(platform);
  CHECK(buffer.bytes() == nullptr);
  CHECK_EQ(buffer.size(), 0u);
  CHECK(buffer.physical_address() == 0u);
  CHECK(buffer.invariants_hold()); }

TEST(dma_buffer_initialize, allocates_zero_filled_device_visible_memory)
{ MockPlatform platform;
  { DmaBuffer buffer(platform);
    CHECK_STATUS(buffer.initialize(128u), Status::ok);
    CHECK(buffer.invariants_hold());
    CHECK_EQ(buffer.size(), 128u);
    CHECK(buffer.bytes() != nullptr);
    CHECK(platform.translate_physical(buffer.physical_address(), 128u) == buffer.bytes());
    bool zero = true;
    for (std::size_t i = 0u; i < 128u; i++) zero = zero && buffer.bytes()[i] == 0u;
    CHECK(zero); }
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(dma_buffer_initialize, propagates_allocation_failure)
{ MockPlatform platform; DmaBuffer buffer(platform);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(buffer.initialize(64u), Status::no_memory);
  CHECK(buffer.bytes() == nullptr);
  CHECK(buffer.invariants_hold()); }

TEST(dma_buffer_initialize, rejects_zero_bytes_and_a_second_initialize)
{ MockPlatform platform; DmaBuffer buffer(platform);
  CHECK_STATUS(buffer.initialize(0u), Status::invalid_parameter);
  CHECK_STATUS(buffer.initialize(64u), Status::ok);
  CHECK_STATUS(buffer.initialize(64u), Status::invalid_state);
  CHECK_EQ(buffer.size(), 64u); }

TEST(dma_buffer_initialize, is_rejected_at_dispatch)
{ MockPlatform platform; DmaBuffer buffer(platform);
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(buffer.initialize(64u), Status::no_memory);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::allocation_at_dispatch)); }

TEST(dma_buffer_invariants, a_size_without_memory_is_detected)
{ MockPlatform platform; DmaBuffer buffer(platform);
  DmaBufferProbe probe(buffer);
  probe.set_size(4u);
  CHECK(!buffer.invariants_hold());
  probe.set_size(0u);
  probe.set_physical_address(0x1000u);
  CHECK(!buffer.invariants_hold()); }
