#include "executor/opencl/cl_freed_memory.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

TEST(cl_freed_memory, nothing_is_freed_at_first_and_what_is_added_accumulates)
{ av::ClFreedMemory freed;
  CHECK_EQ(freed.total(), 0u);
  freed.add(4096u);
  freed.add(1u << 20);
  CHECK_EQ(freed.total(), 4096u + (1u << 20)); }

TEST(cl_freed_memory, a_total_that_would_pass_the_largest_count_stops_there)
{ av::ClFreedMemory freed;
  freed.add(av::ClFreedMemory::saturated - 5u);
  freed.add(4u);
  CHECK_EQ(freed.total(), av::ClFreedMemory::saturated - 1u);
  freed.add(100u);
  CHECK_EQ(freed.total(), av::ClFreedMemory::saturated);
  freed.add(1u);
  CHECK_EQ(freed.total(), av::ClFreedMemory::saturated); }
