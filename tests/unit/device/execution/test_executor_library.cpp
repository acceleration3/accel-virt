/* The table an executor library hands over: accepted only whole and of this
 * ABI, and what it offers limited to the sub-APIs the protocol knows. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/execution/executor_library.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/sub_api.hpp"

#include <cstdint>

using av::ExecutorLibrary;
using av::MockExecutorOps;
using av::Status;
using av::SubApi;

TEST(executor_library_accept, a_whole_table_of_this_abi_is_available)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL);
  ExecutorLibrary library;
  CHECK(!library.available());
  CHECK_EQ(library.sub_apis(), 0u);
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK(library.available());
  CHECK_EQ(library.sub_apis(), RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL);
  CHECK_STATUS(library.accept(&ops.table()), Status::invalid_state); }

TEST(executor_library_accept, no_table_leaves_the_device_without_an_executor)
{ ExecutorLibrary library;
  CHECK_STATUS(library.accept(nullptr), Status::ok);
  CHECK(!library.available());
  CHECK_EQ(library.sub_apis(), 0u); }

TEST(executor_library_accept, another_abi_version_or_a_missing_operation_is_refused)
{ for (int broken = 0; broken < 5; broken++) {
      MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
      RvkExecutorOps &table = ops.editable();
      if (broken == 0) table.abi_version = RVK_EXECUTOR_ABI_VERSION + 1u;
      if (broken == 1) table.sub_apis = nullptr;
      if (broken == 2) table.create = nullptr;
      if (broken == 3) table.destroy = nullptr;
      if (broken == 4) table.execute = nullptr;
      ExecutorLibrary library;
      CHECK_STATUS(library.accept(&table), Status::invalid_parameter);
      CHECK(!library.available());
  } }

TEST(executor_library_sub_apis, bits_the_protocol_does_not_know_are_not_offered)
{ MockExecutorOps ops(0xFFFFFFFFu);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK_EQ(library.sub_apis(), 7u); }

/* A version 1 library keeps working and imports nothing; a version 2 one must
 * carry all three memory operations or it is not version 2 (feature 042). */
TEST(executor_library_accept, version_one_is_still_a_library_that_imports_nothing)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  RvkExecutorOps &table = ops.editable();
  table.abi_version = RVK_EXECUTOR_ABI_VERSION_MINIMUM;
  table.import_kinds = nullptr;
  table.import_blob = nullptr;
  table.release_blob = nullptr;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&table), Status::ok);
  CHECK(library.available());
  CHECK(!library.imports());
  CHECK_EQ(library.import_kinds(SubApi::vulkan, nullptr), 0u); }

TEST(executor_library_accept, version_two_without_its_operations_is_refused)
{ for (int missing = 0; missing < 3; missing++) {
      MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
      RvkExecutorOps &table = ops.editable();
      if (missing == 0) table.import_kinds = nullptr;
      if (missing == 1) table.import_blob = nullptr;
      if (missing == 2) table.release_blob = nullptr;
      ExecutorLibrary library;
      CHECK_STATUS(library.accept(&table), Status::invalid_parameter);
      CHECK(!library.available());
  } }

TEST(executor_library_accept, a_version_older_than_one_or_newer_than_this_is_refused)
{ for (int wrong = 0; wrong < 2; wrong++) {
      MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
      RvkExecutorOps &table = ops.editable();
      table.abi_version = wrong == 0 ? RVK_EXECUTOR_ABI_VERSION_MINIMUM - 1u
                                     : RVK_EXECUTOR_ABI_VERSION + 1u;
      ExecutorLibrary library;
      CHECK_STATUS(library.accept(&table), Status::invalid_parameter);
  } }

/* What a version 2 library says it can import is what the capset may claim. */
TEST(executor_library_import_kinds, come_from_the_library_for_one_api_and_one_gpu)
{ const std::uint8_t uuid[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ops.set_import_kinds(RVK_BLOB_HOST_POINTER);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK(library.imports());
  CHECK_EQ(library.import_kinds(SubApi::vulkan, uuid), std::uint32_t{RVK_BLOB_HOST_POINTER}); }
