#include "core/support/status.hpp"
#include "guest/transport/batch_mark.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstdint>

using av::BatchMark;
using av::MockKmt;
using av::Status;

TEST(batch_mark, names_a_batch_only_with_a_device_a_context_and_a_fence)
{ MockKmt kmt;
  constexpr struct {
      std::uint32_t device;
      std::uint32_t context;
      std::uint64_t fence;
  } missing[] = {{0u, 2u, 3u}, {1u, 0u, 3u}, {1u, 2u, 0u}};
  for (const auto &row : missing) {
    const BatchMark mark(row.device, row.context, 4u, row.fence);
    CHECK(!mark.waitable());
    CHECK_STATUS(mark.wait(kmt), Status::invalid_state);
  }
  CHECK(BatchMark(1u, 2u, 0u, 3u).waitable());
  CHECK_EQ(BatchMark(1u, 2u, 4u, 3u).fence(), 3u);
  CHECK_EQ(BatchMark().fence(), 0u);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_hw_queue) + kmt.calls(MockKmt::Call::wait_for_context), 0u); }
