#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/recording_filter.hpp"
#include "device/recording/recording_tool_request.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::RecordHeader;
using av::RecordingFilter;
using av::RecordingToolRequest;
using av::RecordKind;
using av::Status;

namespace {

/* argv as a shell gives it, argv[0] included. */
class Arguments {
public:
    Arguments() : count_(1), values_{"rvkrecord"} {}

    Arguments &add(const char *value)
    {
        CHECK(count_ < static_cast<int>(sizeof values_ / sizeof values_[0]));
        values_[count_] = value;
        count_++;
        return *this;
    }

    int                count() const { return count_; }
    const char *const *values() const { return values_; }

private:
    int         count_;
    const char *values_[16];
};

RecordHeader a_batch(std::uint32_t context, std::uint64_t fence)
{
    return RecordHeader(RecordKind::batch, context, 0u, 0u, fence);
}

}  // namespace

TEST(recording_tool_request_parse, reads_a_print_of_a_recording)
{ RecordingFilter      filter;
  RecordingToolRequest request;
  const Arguments      arguments = Arguments().add("print").add("run.rvk");
  CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::ok);
  CHECK(request.action() == RecordingToolRequest::Action::print);
  CHECK_EQ(std::strcmp(request.recording(), "run.rvk"), 0);
  CHECK(request.executor() == nullptr);
  CHECK(request.gpu() == nullptr);
  CHECK_EQ(request.payload_bytes(), 0u);
  CHECK_EQ(std::strcmp(request.reason(), ""), 0);
  CHECK(request.invariants_hold()); }

TEST(recording_tool_request_parse, a_replay_needs_a_library_and_takes_a_gpu)
{ RecordingFilter      filter;
  RecordingToolRequest without;
  const Arguments      bare = Arguments().add("replay").add("run.rvk");
  CHECK_STATUS(without.parse(bare.count(), bare.values(), filter), Status::invalid_parameter);
  CHECK(std::strstr(without.reason(), "--executor") != nullptr);
  CHECK(without.action() == RecordingToolRequest::Action::none);
  CHECK(without.invariants_hold());

  RecordingToolRequest request;
  const Arguments      full = Arguments().add("replay").add("run.rvk").add("--executor").add("lib.so")
                                  .add("--gpu").add("0123456789abcdefFEDCBA9876543210");
  CHECK_STATUS(request.parse(full.count(), full.values(), filter), Status::ok);
  CHECK(request.action() == RecordingToolRequest::Action::replay);
  CHECK_EQ(std::strcmp(request.executor(), "lib.so"), 0);
  CHECK(request.gpu() != nullptr);
  CHECK_EQ(request.gpu()[0], 0x01u);
  CHECK_EQ(request.gpu()[7], 0xEFu);
  CHECK_EQ(request.gpu()[8], 0xFEu);
  CHECK_EQ(request.gpu()[15], 0x10u);
  CHECK(request.invariants_hold()); }

TEST(recording_tool_request_parse, refuses_a_gpu_that_is_not_thirty_two_hexadecimal_characters)
{ const char *const refused[] = {"", "0123456789abcdef", "0123456789abcdefFEDCBA987654321",
                                 "0123456789abcdefFEDCBA98765432100", "0123456789abcdefFEDCBA987654321g"};
  for (const char *const value : refused) {
      RecordingFilter      filter;
      RecordingToolRequest request;
      const Arguments      arguments =
          Arguments().add("replay").add("run.rvk").add("--executor").add("lib.so").add("--gpu").add(value);
      CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::invalid_parameter);
      CHECK(std::strstr(request.reason(), "--gpu") != nullptr);
      CHECK(request.gpu() == nullptr);
      CHECK(request.invariants_hold());
  } }

TEST(recording_tool_request_parse, the_filter_flags_narrow_the_filter_they_are_given)
{ RecordingFilter      filter;
  RecordingToolRequest request;
  const Arguments      arguments = Arguments().add("print").add("run.rvk").add("--context").add("7")
                                       .add("--api").add("vulkan").add("--fence").add("3:9")
                                       .add("--command").add("Create").add("--bytes").add("16");
  CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::ok);
  CHECK_EQ(filter.context(), 7u);
  CHECK_EQ(filter.api(), 1u);
  CHECK(filter.names_a_command());
  CHECK(filter.keeps(a_batch(7u, 3u)));
  CHECK(filter.keeps(a_batch(7u, 9u)));
  CHECK(!filter.keeps(a_batch(7u, 10u)));
  CHECK(!filter.keeps(a_batch(8u, 3u)));
  CHECK_EQ(request.payload_bytes(), 16u);
  CHECK(filter.invariants_hold());
  CHECK(request.invariants_hold()); }

TEST(recording_tool_request_parse, a_single_fence_is_a_range_of_one)
{ RecordingFilter      filter;
  RecordingToolRequest request;
  const Arguments      arguments = Arguments().add("print").add("run.rvk").add("--fence").add("12");
  CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::ok);
  CHECK(filter.keeps(a_batch(1u, 12u)));
  CHECK(!filter.keeps(a_batch(1u, 11u)));
  CHECK(!filter.keeps(a_batch(1u, 13u))); }

TEST(recording_tool_request_parse, refuses_a_fence_that_is_no_range)
{ const char *const refused[] = {"", ":", "3:", ":9", "9:3", "x", "3:x", "1234567890123456789012345",
                                 "99999999999999999999999999"};
  for (const char *const value : refused) {
      RecordingFilter      filter;
      RecordingToolRequest request;
      const Arguments      arguments = Arguments().add("print").add("run.rvk").add("--fence").add(value);
      CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::invalid_parameter);
      CHECK(std::strstr(request.reason(), "--fence") != nullptr);
      CHECK(request.invariants_hold());
  } }

TEST(recording_tool_request_parse, refuses_what_it_does_not_understand)
{ struct Case { Arguments arguments; const char *reason; };
  const Case cases[] = {
      {Arguments(), "usage"},
      {Arguments().add("print"), "usage"},
      {Arguments().add("show").add("run.rvk"), "print or replay"},
      {Arguments().add("print").add("run.rvk").add("--context"), "no value"},
      {Arguments().add("print").add("run.rvk").add("--nonsense").add("1"), "does not know"},
      {Arguments().add("print").add("run.rvk").add("--context").add("x"), "--context"},
      {Arguments().add("print").add("run.rvk").add("--context").add("4294967296"), "--context"},
      {Arguments().add("print").add("run.rvk").add("--bytes").add("-1"), "--bytes"},
      {Arguments().add("print").add("run.rvk").add("--bytes").add("4294967296"), "--bytes"},
      {Arguments().add("print").add("run.rvk").add("--api").add("d3d"), "--api"},
  };
  for (const Case &c : cases) {
      RecordingFilter      filter;
      RecordingToolRequest request;
      CHECK_STATUS(request.parse(c.arguments.count(), c.arguments.values(), filter),
                   Status::invalid_parameter);
      CHECK(std::strstr(request.reason(), c.reason) != nullptr);
      CHECK(request.action() == RecordingToolRequest::Action::none);
      CHECK(request.invariants_hold());
  }
  /* No arguments at all, which is what a program started with none has. */
  RecordingFilter      filter;
  RecordingToolRequest none;
  CHECK_STATUS(none.parse(0, nullptr, filter), Status::invalid_parameter);
  CHECK(none.invariants_hold()); }

TEST(recording_tool_request_parse, refuses_a_command_fragment_longer_than_any_name)
{ char longest[RecordingFilter::longest_fragment + 4u];
  for (std::size_t index = 0u; index + 1u < sizeof longest; index++) longest[index] = 'a';
  longest[sizeof longest - 1u] = '\0';
  RecordingFilter      filter;
  RecordingToolRequest request;
  const Arguments      arguments = Arguments().add("print").add("run.rvk").add("--command").add(longest);
  CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::invalid_parameter);
  CHECK(std::strstr(request.reason(), "--command") != nullptr);
  CHECK(request.invariants_hold()); }
