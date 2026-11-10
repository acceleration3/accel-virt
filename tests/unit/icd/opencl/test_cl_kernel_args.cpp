#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_kernel_args.hpp"
#include "icd/opencl/cl_object.hpp"

#include <cstdint>
#include <cstring>

using av::Status;
using av::clicd::ClKernelArgs;
using av::clicd::ClObject;

namespace {

ClObject *object(std::uintptr_t number)
{
    return reinterpret_cast<ClObject *>(number * 0x100u);
}

}

TEST(cl_kernel_args, a_new_record_holds_no_argument)
{ av::MockPlatform memory;
  ClKernelArgs     args(memory);
  CHECK_EQ(args.count(), 0u);
  CHECK(args.invariants_hold()); }

TEST(cl_kernel_args, a_value_is_kept_by_its_bytes_and_replaced_by_the_next_one_for_its_index)
{ av::MockPlatform memory;
  ClKernelArgs     args(memory);
  const std::uint32_t first = 0xAABBCCDDu;
  const std::uint64_t second = 0x1122334455667788ull;
  CHECK_STATUS(args.set_value(2u, &first, sizeof first), Status::ok);
  CHECK_STATUS(args.set_value(5u, &second, sizeof second), Status::ok);
  CHECK_EQ(args.count(), 2u);
  CHECK_EQ(args.at(0u).index, 2u);
  CHECK_EQ(args.at(0u).bytes, sizeof first);
  CHECK(std::memcmp(args.bytes_of(args.at(0u)), &first, sizeof first) == 0);
  CHECK(std::memcmp(args.bytes_of(args.at(1u)), &second, sizeof second) == 0);
  const std::uint32_t replaced = 7u;
  CHECK_STATUS(args.set_value(2u, &replaced, sizeof replaced), Status::ok);
  CHECK_EQ(args.count(), 2u);
  CHECK(std::memcmp(args.bytes_of(args.at(0u)), &replaced, sizeof replaced) == 0);
  CHECK(args.invariants_hold()); }

TEST(cl_kernel_args, an_object_and_a_local_size_take_the_place_of_a_value_at_their_index)
{ av::MockPlatform memory;
  ClKernelArgs     args(memory);
  const std::uint32_t value = 3u;
  CHECK_STATUS(args.set_value(0u, &value, sizeof value), Status::ok);
  CHECK_STATUS(args.set_object(0u, object(1u)), Status::ok);
  CHECK_EQ(args.count(), 1u);
  CHECK(args.at(0u).kind == ClKernelArgs::Kind::object);
  CHECK(args.at(0u).object == object(1u));
  CHECK(args.bytes_of(args.at(0u)) == nullptr);
  CHECK_STATUS(args.set_local(0u, 256u), Status::ok);
  CHECK(args.at(0u).kind == ClKernelArgs::Kind::local);
  CHECK_EQ(args.at(0u).bytes, 256u);
  CHECK(args.invariants_hold()); }

TEST(cl_kernel_args, an_argument_with_nothing_in_it_is_refused)
{ av::MockPlatform memory;
  ClKernelArgs     args(memory);
  const std::uint32_t value = 3u;
  CHECK_STATUS(args.set_value(0u, nullptr, 4u), Status::invalid_parameter);
  CHECK_STATUS(args.set_value(0u, &value, 0u), Status::invalid_parameter);
  CHECK_STATUS(args.set_object(0u, nullptr), Status::invalid_parameter);
  CHECK_STATUS(args.set_local(0u, 0u), Status::invalid_parameter);
  CHECK_EQ(args.count(), 0u); }

TEST(cl_kernel_args, a_copy_holds_what_the_original_held_and_none_of_what_it_held_before)
{ av::MockPlatform memory;
  ClKernelArgs     original(memory);
  ClKernelArgs     copy(memory);
  const std::uint32_t value = 0x01020304u;
  const std::uint32_t before = 9u;
  CHECK_STATUS(copy.set_value(7u, &before, sizeof before), Status::ok);
  CHECK_STATUS(original.set_value(1u, &value, sizeof value), Status::ok);
  CHECK_STATUS(original.set_object(2u, object(4u)), Status::ok);
  CHECK_STATUS(original.set_local(3u, 64u), Status::ok);
  CHECK_STATUS(copy.copy_from(original), Status::ok);
  CHECK_EQ(copy.count(), 3u);
  CHECK(std::memcmp(copy.bytes_of(copy.at(0u)), &value, sizeof value) == 0);
  CHECK(copy.at(1u).object == object(4u));
  CHECK_EQ(copy.at(2u).bytes, 64u);
  CHECK(copy.invariants_hold()); }

TEST(cl_kernel_args, memory_that_runs_out_leaves_the_record_as_it_was)
{ bool refused = false;
  for (std::uint32_t nth = 1u; nth <= 6u; ++nth) {
      av::MockPlatform memory;
      ClKernelArgs     args(memory);
      const std::uint32_t first = 1u;
      CHECK_STATUS(args.set_value(0u, &first, sizeof first), Status::ok);
      memory.fail_allocation_at(nth);
      const std::uint64_t big[64] = {};
      const Status        set = args.set_value(1u, big, sizeof big);
      memory.fail_allocation_at(0u);
      if (set != Status::ok) {
          refused = true;
          CHECK_EQ(args.count(), 1u);
      }
      CHECK(args.invariants_hold());
  }
  CHECK(refused); }
