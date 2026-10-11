/* The guest devices the library serves, each a domain on one host GPU: what
 * keeps two adapters' shared ids apart. */
#include "core/trace/state_writer.hpp"
#include "executor/executor_domains.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::ExecutorDomains;
using av::StateWriter;

TEST(executor_domains, a_domain_names_the_gpu_it_was_opened_on)
{ ExecutorDomains domains;
  const std::uint32_t first = domains.open(0u);
  const std::uint32_t second = domains.open(3u);
  CHECK(first != 0u);
  CHECK(second != 0u);
  CHECK(first != second);
  std::uint32_t gpu = 9u;
  CHECK(domains.gpu_of(first, gpu));
  CHECK_EQ(gpu, 0u);
  CHECK(domains.gpu_of(second, gpu));
  CHECK_EQ(gpu, 3u);
  CHECK_EQ(domains.live(), 2u); }

/* A device that asks with a domain it closed, or one never opened, is told it
 * names nothing: ids are not reused, so a stale one cannot reach another
 * device's shares. */
TEST(executor_domains, a_closed_domain_names_nothing_and_is_never_reused)
{ ExecutorDomains domains;
  const std::uint32_t first = domains.open(2u);
  std::uint32_t gpu = 9u;
  CHECK(domains.close(first, gpu));
  CHECK_EQ(gpu, 2u);
  CHECK(!domains.gpu_of(first, gpu));
  CHECK_EQ(gpu, 0u);
  CHECK(!domains.close(first, gpu));
  CHECK(!domains.close(0u, gpu));
  CHECK(!domains.gpu_of(0u, gpu));
  CHECK(!domains.gpu_of(first + 100u, gpu));
  const std::uint32_t again = domains.open(2u);
  CHECK(again != first);
  CHECK_EQ(domains.live(), 1u); }

TEST(executor_domains, every_entry_taken_is_a_refusal)
{ ExecutorDomains domains;
  std::uint32_t last = 0u;
  for (std::uint32_t entry = 0u; entry < ExecutorDomains::max_domains; entry++) last = domains.open(entry);
  CHECK_EQ(domains.open(0u), 0u);
  std::uint32_t gpu = 0u;
  CHECK(domains.close(last, gpu));
  CHECK(domains.open(0u) != 0u); }

TEST(executor_domains, reports_each_open_domain_and_its_gpu)
{ ExecutorDomains domains;
  const std::uint32_t first = domains.open(1u);
  const std::uint32_t closed = domains.open(2u);
  std::uint32_t gpu = 0u;
  CHECK(domains.close(closed, gpu));
  static char bytes[512];
  StateWriter writer(bytes, sizeof bytes);
  domains.report(writer);
  CHECK(std::strstr(writer.dump(), "gpu=1") != nullptr);
  CHECK(std::strstr(writer.dump(), "gpu=2") == nullptr);
  CHECK_EQ(first, 1u); }
