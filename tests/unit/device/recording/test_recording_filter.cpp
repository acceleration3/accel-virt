#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/recorded_command.hpp"
#include "device/recording/recording_filter.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>

using av::CommandHeader;
using av::RecordedCommand;
using av::RecordHeader;
using av::RecordingFilter;
using av::RecordKind;
using av::Status;

namespace {

RecordHeader a_batch(std::uint32_t context, std::uint64_t fence)
{
    return RecordHeader(RecordKind::batch, context, 0u, 0u, fence);
}

/* One framed command, for the filter to judge. */
class OneCommand {
public:
    explicit OneCommand(std::uint32_t id) : bytes_{}
    {
        CHECK_STATUS(CommandHeader(id, CommandHeader::encoded_size, 0u).encode(bytes_, sizeof bytes_),
                     Status::ok);
        CHECK_STATUS(command_.decode(bytes_, sizeof bytes_, 0u), Status::ok);
    }

    const RecordedCommand &command() const { return command_; }

private:
    std::uint8_t    bytes_[CommandHeader::encoded_size];
    RecordedCommand command_;
};

}  // namespace

namespace av {

struct RecordingFilterProbe {
    /* A range no keep_fences would take, for the invariant to catch. */
    static void invert(RecordingFilter &filter) { filter.first_fence_ = 9u; filter.last_fence_ = 8u; }
    static void unterminate(RecordingFilter &filter)
    {
        filter.fragment_[RecordingFilter::longest_fragment - 1u] = 'x';
    }
};

}  // namespace av

TEST(recording_filter, keeps_everything_until_it_is_told_otherwise)
{ const RecordingFilter filter;
  CHECK(filter.invariants_hold());
  CHECK_EQ(filter.context(), RecordingFilter::any_context);
  CHECK_EQ(filter.api(), RecordingFilter::any_api);
  CHECK(!filter.names_a_command());
  CHECK(filter.keeps(a_batch(1u, 1u)));
  CHECK(filter.keeps(a_batch(99u, ~std::uint64_t{0})));
  CHECK(filter.keeps(OneCommand(0x00010006u).command()));
  CHECK(filter.keeps(OneCommand(CommandHeader::nop_id).command())); }

TEST(recording_filter_keep_context, keeps_one_context_and_no_other)
{ RecordingFilter filter;
  filter.keep_context(4u);
  CHECK_EQ(filter.context(), 4u);
  CHECK(filter.keeps(a_batch(4u, 1u)));
  CHECK(!filter.keeps(a_batch(5u, 1u)));
  /* Context 0 is the device's own, and a filter for it keeps only that. */
  filter.keep_context(0u);
  CHECK(filter.keeps(a_batch(0u, 0u)));
  CHECK(!filter.keeps(a_batch(4u, 1u)));
  CHECK(filter.invariants_hold()); }

TEST(recording_filter_keep_api, names_each_sub_api_and_refuses_anything_else)
{ struct Case { const char *name; std::uint16_t api; };
  const Case cases[] = {{"transport", 0u}, {"vulkan", 1u}, {"opencl", 2u}, {"opengl", 3u}};
  for (const Case &c : cases) {
      RecordingFilter filter;
      CHECK_STATUS(filter.keep_api(c.name), Status::ok);
      CHECK_EQ(filter.api(), c.api);
      CHECK(filter.invariants_hold());
  }
  RecordingFilter filter;
  CHECK_STATUS(filter.keep_api("vulkan"), Status::ok);
  const char *const refused[] = {"", "vulcan", "vulkans", "VULKAN", "vul", nullptr};
  for (const char *const name : refused) {
      CHECK_STATUS(filter.keep_api(name), Status::invalid_parameter);
      CHECK_EQ(filter.api(), 1u);
  }
  CHECK(filter.keeps(OneCommand(0x00010006u).command()));
  CHECK(!filter.keeps(OneCommand(0x00020001u).command())); }

TEST(recording_filter_keep_fences, keeps_a_range_and_refuses_one_that_ends_before_it_starts)
{ RecordingFilter filter;
  CHECK_STATUS(filter.keep_fences(10u, 20u), Status::ok);
  CHECK(!filter.keeps(a_batch(1u, 9u)));
  CHECK(filter.keeps(a_batch(1u, 10u)));
  CHECK(filter.keeps(a_batch(1u, 20u)));
  CHECK(!filter.keeps(a_batch(1u, 21u)));
  /* A record belonging to no batch is inside every range. */
  CHECK(filter.keeps(a_batch(1u, 0u)));
  CHECK_STATUS(filter.keep_fences(20u, 19u), Status::invalid_parameter);
  CHECK(filter.keeps(a_batch(1u, 15u)));
  CHECK(filter.invariants_hold());
  CHECK_STATUS(filter.keep_fences(7u, 7u), Status::ok);
  CHECK(filter.keeps(a_batch(1u, 7u)));
  CHECK(!filter.keeps(a_batch(1u, 8u))); }

TEST(recording_filter_keep_command, keeps_a_command_whose_name_holds_the_fragment)
{ RecordingFilter filter;
  CHECK_STATUS(filter.keep_command("Create"), Status::ok);
  CHECK(filter.names_a_command());
  CHECK(filter.keeps(OneCommand(0x00010006u).command()));    /* vkCreateInstance */
  CHECK(!filter.keeps(OneCommand(CommandHeader::nop_id).command()));
  CHECK_STATUS(filter.keep_command("nop"), Status::ok);
  CHECK(filter.keeps(OneCommand(CommandHeader::nop_id).command()));
  CHECK(!filter.keeps(OneCommand(0x00010006u).command()));
  /* The whole name, and a fragment at its very end. */
  CHECK_STATUS(filter.keep_command("vkCreateInstance"), Status::ok);
  CHECK(filter.keeps(OneCommand(0x00010006u).command()));
  CHECK_STATUS(filter.keep_command("Instance"), Status::ok);
  CHECK(filter.keeps(OneCommand(0x00010006u).command()));
  CHECK_STATUS(filter.keep_command("vkCreateInstanceX"), Status::ok);
  CHECK(!filter.keeps(OneCommand(0x00010006u).command()));
  CHECK(filter.invariants_hold()); }

TEST(recording_filter_keep_command, an_empty_fragment_keeps_everything_and_a_long_one_is_refused)
{ RecordingFilter filter;
  CHECK_STATUS(filter.keep_command("Create"), Status::ok);
  CHECK_STATUS(filter.keep_command(""), Status::ok);
  CHECK(!filter.names_a_command());
  CHECK(filter.keeps(OneCommand(CommandHeader::nop_id).command()));
  char longest[RecordingFilter::longest_fragment + 8u];
  for (std::size_t index = 0u; index + 1u < sizeof longest; index++) longest[index] = 'a';
  longest[sizeof longest - 1u] = '\0';
  CHECK_STATUS(filter.keep_command(longest), Status::invalid_parameter);
  CHECK_STATUS(filter.keep_command(nullptr), Status::invalid_parameter);
  CHECK(!filter.names_a_command());
  /* One shorter than the limit is kept. */
  longest[RecordingFilter::longest_fragment - 1u] = '\0';
  CHECK_STATUS(filter.keep_command(longest), Status::ok);
  CHECK(filter.names_a_command());
  CHECK(filter.invariants_hold()); }

TEST(recording_filter, narrows_on_every_axis_at_once)
{ RecordingFilter filter;
  filter.keep_context(3u);
  CHECK_STATUS(filter.keep_fences(5u, 9u), Status::ok);
  CHECK_STATUS(filter.keep_api("vulkan"), Status::ok);
  CHECK_STATUS(filter.keep_command("Create"), Status::ok);
  CHECK(filter.keeps(a_batch(3u, 7u)));
  CHECK(!filter.keeps(a_batch(3u, 4u)));
  CHECK(!filter.keeps(a_batch(2u, 7u)));
  CHECK(filter.keeps(OneCommand(0x00010006u).command()));
  CHECK(!filter.keeps(OneCommand(0x00020001u).command()));   /* the right name, the wrong API */
  CHECK(filter.invariants_hold()); }

TEST(recording_filter_invariants, catch_a_range_that_ends_before_it_starts_and_a_fragment_with_no_end)
{ RecordingFilter filter;
  CHECK(filter.invariants_hold());
  av::RecordingFilterProbe::invert(filter);
  CHECK(!filter.invariants_hold());
  CHECK_STATUS(filter.keep_fences(1u, 2u), Status::ok);
  CHECK(filter.invariants_hold());
  av::RecordingFilterProbe::unterminate(filter);
  CHECK(!filter.invariants_hold()); }
