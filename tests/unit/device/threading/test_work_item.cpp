#include "device/threading/work_item.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

using av::MockHost;
using av::WorkItem;

TEST(work_item, runs_its_work_now_and_its_completion_on_the_main_loop)
{ MockHost host;
  std::uint32_t calls = 0u;
  const WorkItem item([](void *count) { *static_cast<std::uint32_t *>(count) += 1u; },
                      [](void *count) { *static_cast<std::uint32_t *>(count) += 10u; }, &calls);
  item.run();
  CHECK_EQ(calls, 1u);
  item.complete_on(host);
  CHECK_EQ(calls, 1u);
  CHECK_EQ(host.run_main_loop(), 1u);
  CHECK_EQ(calls, 11u); }

TEST(work_item, without_a_completion_nothing_reaches_the_main_loop)
{ MockHost host;
  std::uint32_t calls = 0u;
  const WorkItem item([](void *count) { *static_cast<std::uint32_t *>(count) += 1u; }, nullptr, &calls);
  item.run();
  item.complete_on(host);
  CHECK_EQ(host.run_main_loop(), 0u);
  CHECK_EQ(calls, 1u); }
