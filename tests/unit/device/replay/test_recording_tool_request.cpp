#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "device/replay/recording_filter.hpp"
#include "device/replay/recording_tool_request.hpp"
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
class Arguments {
public:
    Arguments() : count_(1), values_{"avrecord"} {}

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

}

namespace av {

struct RecordingToolRequestProbe {
    static void refuse_silently(RecordingToolRequest &request) { request.reason_ = ""; }
    static void forget_the_recording(RecordingToolRequest &request) { request.recording_ = nullptr; }
    static void refuse_after_the_fact(RecordingToolRequest &request) { request.reason_ = "later"; }
    static void forget_the_library(RecordingToolRequest &request) { request.executor_ = nullptr; }
};

}

TEST(recording_tool_request_parse, reads_a_print_of_a_recording)
{ RecordingFilter      filter;
  RecordingToolRequest request;
  const Arguments      arguments = Arguments().add("print").add("run.av");
  CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::ok);
  CHECK(request.action() == RecordingToolRequest::Action::print);
  CHECK_EQ(std::strcmp(request.recording(), "run.av"), 0);
  CHECK(request.executor() == nullptr);
  CHECK(request.gpu() == nullptr);
  CHECK_EQ(request.payload_bytes(), 0u);
  CHECK_EQ(std::strcmp(request.reason(), ""), 0);
  CHECK(request.invariants_hold()); }

TEST(recording_tool_request_parse, a_replay_needs_a_library_and_takes_a_gpu)
{ RecordingFilter      filter;
  RecordingToolRequest without;
  const Arguments      bare = Arguments().add("replay").add("run.av");
  CHECK_STATUS(without.parse(bare.count(), bare.values(), filter), Status::invalid_parameter);
  CHECK(std::strstr(without.reason(), "--executor") != nullptr);
  CHECK(without.action() == RecordingToolRequest::Action::none);
  CHECK(without.invariants_hold());

  RecordingToolRequest request;
  const Arguments      full = Arguments().add("replay").add("run.av").add("--executor").add("lib.so")
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
          Arguments().add("replay").add("run.av").add("--executor").add("lib.so").add("--gpu").add(value);
      CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::invalid_parameter);
      CHECK(std::strstr(request.reason(), "--gpu") != nullptr);
      CHECK(request.gpu() == nullptr);
      CHECK(request.invariants_hold());
  } }

TEST(recording_tool_request_parse, the_filter_flags_narrow_the_filter_they_are_given)
{ RecordingFilter      filter;
  RecordingToolRequest request;
  const Arguments      arguments = Arguments().add("print").add("run.av").add("--context").add("7")
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
  const Arguments      arguments = Arguments().add("print").add("run.av").add("--fence").add("12");
  CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::ok);
  CHECK(filter.keeps(a_batch(1u, 12u)));
  CHECK(!filter.keeps(a_batch(1u, 11u)));
  CHECK(!filter.keeps(a_batch(1u, 13u))); }

TEST(recording_tool_request_parse, refuses_a_fence_that_is_no_range)
{ struct Case { const char *value; const char *reason; };
  const Case refused[] = {{"", "wants a fence or a range"},
                          {":", "wants a fence or a range"},
                          {"3:", "wants a fence or a range"},
                          {":9", "wants a fence or a range"},
                          {"x", "wants a fence or a range"},
                          {"3:x", "wants a fence or a range"},
                          {"1234567890123456789012345", "wants a fence or a range"},
                          {"99999999999999999999999999", "wants a fence or a range"},
                          {"9:3", "ends before it starts"}};
  for (const Case &c : refused) {
      RecordingFilter      filter;
      RecordingToolRequest request;
      const Arguments      arguments = Arguments().add("print").add("run.av").add("--fence").add(c.value);
      CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::invalid_parameter);
      CHECK(std::strstr(request.reason(), "--fence") != nullptr);
      CHECK(std::strstr(request.reason(), c.reason) != nullptr);
      CHECK(filter.keeps(a_batch(1u, 1u)));
      CHECK(filter.keeps(a_batch(1u, ~std::uint64_t{0})));
      CHECK(request.invariants_hold());
  } }

TEST(recording_tool_request_parse, takes_the_largest_fence_and_refuses_the_first_one_past_it)
{ RecordingFilter      filter;
  RecordingToolRequest request;
  const Arguments      largest =
      Arguments().add("print").add("run.av").add("--fence").add("18446744073709551615");
  CHECK_STATUS(request.parse(largest.count(), largest.values(), filter), Status::ok);
  CHECK(filter.keeps(a_batch(1u, ~std::uint64_t{0})));
  CHECK(!filter.keeps(a_batch(1u, ~std::uint64_t{0} - 1u)));

  RecordingFilter      wider;
  RecordingToolRequest past;
  const Arguments      one_more =
      Arguments().add("print").add("run.av").add("--fence").add("18446744073709551616");
  CHECK_STATUS(past.parse(one_more.count(), one_more.values(), wider), Status::invalid_parameter);
  CHECK(std::strstr(past.reason(), "--fence") != nullptr);
  CHECK(wider.keeps(a_batch(1u, 1u))); }

TEST(recording_tool_request_parse, refuses_what_it_does_not_understand)
{ struct Case { Arguments arguments; const char *reason; };
  const Case cases[] = {
      {Arguments(), "usage"},
      {Arguments().add("print"), "usage"},
      {Arguments().add("show").add("run.av"), "print or replay"},
      {Arguments().add("print").add("run.av").add("--context"), "no value"},
      {Arguments().add("print").add("run.av").add("--nonsense").add("1"), "does not know"},
      {Arguments().add("print").add("run.av").add("--context").add("x"), "--context"},
      {Arguments().add("print").add("run.av").add("--context").add("4294967296"), "--context"},
      {Arguments().add("print").add("run.av").add("--bytes").add("-1"), "--bytes"},
      {Arguments().add("print").add("run.av").add("--bytes").add("4294967296"), "--bytes"},
      {Arguments().add("print").add("run.av").add("--api").add("metal"), "--api"},
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
  const Arguments      arguments = Arguments().add("print").add("run.av").add("--command").add(longest);
  CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::invalid_parameter);
  CHECK(std::strstr(request.reason(), "--command") != nullptr);
  CHECK(request.invariants_hold()); }

TEST(recording_tool_request_parse, refuses_arguments_it_was_handed_none_of)
{ RecordingFilter      filter;
  RecordingToolRequest request;
  CHECK_STATUS(request.parse(3, nullptr, filter), Status::invalid_parameter);
  CHECK(std::strstr(request.reason(), "usage") != nullptr);
  CHECK(request.invariants_hold()); }

TEST(recording_tool_request_parse, refuses_a_number_larger_than_sixty_four_bits_hold)
{ RecordingFilter      filter;
  RecordingToolRequest request;
  const Arguments      arguments =
      Arguments().add("print").add("run.av").add("--bytes").add("99999999999999999999");
  CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter), Status::invalid_parameter);
  CHECK(std::strstr(request.reason(), "--bytes") != nullptr);
  CHECK_EQ(request.payload_bytes(), 0u); }

TEST(recording_tool_request_parse, reads_a_gpu_in_either_case_and_refuses_anything_else)
{ struct Case { const char *hex; bool taken; std::uint8_t byte; };
  const Case cases[] = {{"00000000000000000000000000000000", true, 0x00u},
                        {"ffffffffffffffffffffffffffffffff", true, 0xFFu},
                        {"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", true, 0xFFu},
                        {"////////////////////////////////", false, 0u},
                        {":::::::::::::::::::::::::::::::: ", false, 0u},
                        {"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@", false, 0u},
                        {"gggggggggggggggggggggggggggggggg", false, 0u},
                        {"````````````````````````````````", false, 0u}};
  for (const Case &c : cases) {
      RecordingFilter      filter;
      RecordingToolRequest request;
      const Arguments      arguments =
          Arguments().add("replay").add("run.av").add("--executor").add("lib.so").add("--gpu").add(c.hex);
      CHECK_STATUS(request.parse(arguments.count(), arguments.values(), filter),
                   c.taken ? Status::ok : Status::invalid_parameter);
      CHECK((request.gpu() != nullptr) == c.taken);
      if (c.taken) {
          CHECK_EQ(request.gpu()[0], c.byte);
          CHECK_EQ(request.gpu()[15], c.byte);
      }
      CHECK(request.invariants_hold());
  } }

TEST(recording_tool_request_invariants, catch_a_request_that_does_not_say_what_it_is)
{ RecordingFilter      filter;
  RecordingToolRequest refused;
  const Arguments      bad = Arguments().add("print").add("run.av").add("--nonsense").add("1");
  CHECK_STATUS(refused.parse(bad.count(), bad.values(), filter), Status::invalid_parameter);
  CHECK(refused.invariants_hold());
  av::RecordingToolRequestProbe::refuse_silently(refused);
  CHECK(!refused.invariants_hold());
  av::RecordingToolRequestProbe::forget_the_recording(refused);
  CHECK(refused.invariants_hold());

  RecordingToolRequest taken;
  const Arguments      good = Arguments().add("print").add("run.av");
  CHECK_STATUS(taken.parse(good.count(), good.values(), filter), Status::ok);
  CHECK(taken.invariants_hold());
  av::RecordingToolRequestProbe::forget_the_recording(taken);
  CHECK(!taken.invariants_hold());

  RecordingToolRequest again;
  CHECK_STATUS(again.parse(good.count(), good.values(), filter), Status::ok);
  av::RecordingToolRequestProbe::refuse_after_the_fact(again);
  CHECK(!again.invariants_hold());

  RecordingToolRequest replaying;
  const Arguments      library =
      Arguments().add("replay").add("run.av").add("--executor").add("lib.so");
  CHECK_STATUS(replaying.parse(library.count(), library.values(), filter), Status::ok);
  CHECK(replaying.invariants_hold());
  av::RecordingToolRequestProbe::forget_the_library(replaying);
  CHECK(!replaying.invariants_hold()); }
