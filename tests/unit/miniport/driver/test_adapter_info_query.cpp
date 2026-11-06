#include "miniport/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "miniport/driver/adapter_info_query.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterInfoQuery;
using av::DxgkargQueryAdapterInfoRecord;
using av::GuardedBuffer;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint32_t payload_bytes = 12u;
constexpr std::uint8_t  payload[payload_bytes] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
constexpr std::uint8_t  untouched = 0xEEu;

DxgkargQueryAdapterInfoRecord record_with_output(std::uint8_t *output, std::uint32_t size)
{
    DxgkargQueryAdapterInfoRecord record = {};
    record.Type = 1;
    record.pOutputData = output;
    record.OutputDataSize = size;
    return record;
}

}

TEST(adapter_info_query, reports_the_requested_type)
{ MockPlatform platform;
  DxgkargQueryAdapterInfoRecord record = {};
  record.Type = 47;
  CHECK_EQ(AdapterInfoQuery(platform, record).type(), 47); }

TEST(adapter_info_query_require_input, accepts_at_least_the_bytes_asked_for)
{ MockPlatform platform; std::uint32_t input = 0u;
  DxgkargQueryAdapterInfoRecord record = {};
  record.pInputData = &input;
  record.InputDataSize = 4u;
  const AdapterInfoQuery query(platform, record);
  CHECK_STATUS(query.require_input(4u), Status::ok);
  CHECK_STATUS(query.require_input(3u), Status::ok); }

TEST(adapter_info_query_require_input, rejects_a_short_or_missing_input)
{ MockPlatform platform; std::uint32_t input = 0u;
  DxgkargQueryAdapterInfoRecord record = {};
  record.pInputData = &input;
  record.InputDataSize = 3u;
  CHECK_STATUS(AdapterInfoQuery(platform, record).require_input(4u), Status::invalid_parameter);
  record.pInputData = nullptr;
  record.InputDataSize = 4u;
  CHECK_STATUS(AdapterInfoQuery(platform, record).require_input(4u), Status::invalid_parameter); }

TEST(adapter_info_query_require_input, nothing_asked_needs_no_input)
{ MockPlatform platform;
  const DxgkargQueryAdapterInfoRecord record = {};
  CHECK_STATUS(AdapterInfoQuery(platform, record).require_input(0u), Status::ok); }

TEST(adapter_info_query_write_output, writes_exactly_the_bytes_into_a_buffer_of_that_size)
{ MockPlatform platform; GuardedBuffer output(payload_bytes);
  const DxgkargQueryAdapterInfoRecord record = record_with_output(output.data(), payload_bytes);
  CHECK_STATUS(AdapterInfoQuery(platform, record).write_output(payload, payload_bytes), Status::ok);
  CHECK(std::memcmp(output.data(), payload, payload_bytes) == 0);
  CHECK(output.guards_intact()); }

TEST(adapter_info_query_write_output, a_larger_output_keeps_the_bytes_past_the_answer)
{ MockPlatform platform; std::uint8_t output[payload_bytes + 4u];
  std::memset(output, untouched, sizeof output);
  const DxgkargQueryAdapterInfoRecord record = record_with_output(output, sizeof output);
  CHECK_STATUS(AdapterInfoQuery(platform, record).write_output(payload, payload_bytes), Status::ok);
  CHECK(std::memcmp(output, payload, payload_bytes) == 0);
  CHECK_EQ(output[payload_bytes], untouched);
  CHECK_EQ(output[payload_bytes + 3u], untouched); }

TEST(adapter_info_query_write_output, a_short_or_missing_output_is_invalid_and_untouched)
{ MockPlatform platform;
  struct Case { std::uint32_t declared; bool null_output; };
  const Case cases[] = {{0u, false}, {1u, false}, {payload_bytes - 1u, false},
                        {payload_bytes, true}, {0xFFFFFFFFu, true}};
  for (const Case &c : cases) {
      GuardedBuffer output(payload_bytes);
      std::memset(output.data(), untouched, payload_bytes);
      const DxgkargQueryAdapterInfoRecord record =
          record_with_output(c.null_output ? nullptr : output.data(), c.declared);
      CHECK_STATUS(AdapterInfoQuery(platform, record).write_output(payload, payload_bytes), Status::invalid_parameter);
      CHECK_EQ(output.data()[0], untouched);
      CHECK_EQ(output.data()[payload_bytes - 1u], untouched);
      CHECK(output.guards_intact());
  } }

TEST(adapter_info_query, is_passive_only)
{ MockPlatform platform; std::uint8_t output[payload_bytes] = {};
  const DxgkargQueryAdapterInfoRecord record = record_with_output(output, payload_bytes);
  const AdapterInfoQuery query(platform, record);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(query.type());
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK_STATUS(query.require_input(0u), Status::ok);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK_STATUS(query.write_output(payload, payload_bytes), Status::ok);
  CHECK(platform.claim(Violation::irql_ceiling));
  std::uint8_t copy[payload_bytes] = {};
  CHECK_STATUS(query.read_output(copy, payload_bytes), Status::ok);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK_STATUS(query.read_input(copy, 1u), Status::invalid_parameter);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }

TEST(adapter_info_query_read_input, copies_what_the_caller_supplied)
{ MockPlatform platform; std::uint32_t input = 0x11007u, copy = 0u;
  DxgkargQueryAdapterInfoRecord record = {};
  record.pInputData = &input;
  record.InputDataSize = sizeof input;
  CHECK_STATUS(AdapterInfoQuery(platform, record).read_input(&copy, sizeof copy), Status::ok);
  CHECK_EQ(copy, 0x11007u);
  record.InputDataSize = 3u;
  copy = 0u;
  CHECK_STATUS(AdapterInfoQuery(platform, record).read_input(&copy, sizeof copy), Status::invalid_parameter);
  CHECK_EQ(copy, 0u); }

TEST(adapter_info_query_read_output, copies_an_in_out_block_or_refuses_a_short_one)
{ MockPlatform platform; std::uint8_t output[payload_bytes] = {9, 8, 7}; std::uint8_t copy[payload_bytes] = {};
  DxgkargQueryAdapterInfoRecord record = record_with_output(output, payload_bytes);
  CHECK_STATUS(AdapterInfoQuery(platform, record).read_output(copy, payload_bytes), Status::ok);
  CHECK_EQ(copy[2], 7u);
  record.OutputDataSize = payload_bytes - 1u;
  CHECK_STATUS(AdapterInfoQuery(platform, record).read_output(copy, payload_bytes), Status::invalid_parameter);
  record.pOutputData = nullptr;
  record.OutputDataSize = payload_bytes;
  CHECK_STATUS(AdapterInfoQuery(platform, record).read_output(copy, payload_bytes), Status::invalid_parameter); }

TEST(adapter_info_query_output_size, is_the_block_sized_or_nothing_without_one)
{ MockPlatform platform; std::uint8_t output[payload_bytes] = {};
  DxgkargQueryAdapterInfoRecord record = record_with_output(output, payload_bytes);
  CHECK_EQ(AdapterInfoQuery(platform, record).output_size(), payload_bytes);
  record.pOutputData = nullptr;
  CHECK_EQ(AdapterInfoQuery(platform, record).output_size(), 0u); }
