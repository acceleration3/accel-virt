#include "executor/memory_reuse.hpp"
#include "harness/framework/test_registry.hpp"

using av::MemoryReuse;

TEST(memory_reuse, a_type_nobody_freed_crosses_no_domain)
{ const MemoryReuse reuse;
  CHECK(!reuse.crosses_domains(3u, 1u));
  CHECK(!reuse.crosses_domains(3u, 2u)); }

TEST(memory_reuse, memory_a_domain_freed_crosses_into_every_other_domain_and_not_its_own)
{ MemoryReuse reuse;
  reuse.freed(3u, 1u);
  CHECK(!reuse.crosses_domains(3u, 1u));
  CHECK(reuse.crosses_domains(3u, 2u));
  CHECK(!reuse.crosses_domains(4u, 2u)); }

TEST(memory_reuse, once_two_domains_freed_a_type_crosses_into_both)
{ MemoryReuse reuse;
  reuse.freed(3u, 1u);
  reuse.freed(3u, 2u);
  CHECK(reuse.crosses_domains(3u, 1u));
  CHECK(reuse.crosses_domains(3u, 2u));
  reuse.freed(3u, 1u);
  CHECK(reuse.crosses_domains(3u, 1u)); }

TEST(memory_reuse, a_type_past_the_vulkan_limit_is_always_treated_as_crossing_and_never_recorded)
{ MemoryReuse reuse;
  reuse.freed(32u, 1u);
  CHECK(reuse.crosses_domains(32u, 1u));
  CHECK(reuse.crosses_domains(1000u, 1u)); }
