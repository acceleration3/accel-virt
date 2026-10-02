#include "miniport/display/display_info_response.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::DisplayInfoResponse;
using av::Status;

namespace {

constexpr std::size_t header_size = 24u;
constexpr std::size_t scanout_size = 24u;

void write_u32(std::uint8_t *bytes, std::size_t offset, std::uint32_t value)
{
    for (unsigned byte = 0u; byte < 4u; byte++) bytes[offset + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
}

/* A display-info answer as QEMU writes it: the type, then one rectangle and
 * enabled flag per scanout. */
class Answer {
public:
    Answer() : bytes_{}
    {
        std::memset(bytes_, 0, sizeof bytes_);
        write_u32(bytes_, 0u, DisplayInfoResponse::response_type);
    }

    void scanout(std::uint32_t index, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height,
                 std::uint32_t enabled = 1u)
    {
        const std::size_t at = header_size + index * scanout_size;
        write_u32(bytes_, at, x);
        write_u32(bytes_, at + 4u, y);
        write_u32(bytes_, at + 8u, width);
        write_u32(bytes_, at + 12u, height);
        write_u32(bytes_, at + 16u, enabled);
    }

    void field(std::size_t offset, std::uint32_t value) { write_u32(bytes_, offset, value); }
    const std::uint8_t *bytes() const { return bytes_; }
    std::size_t         length() const { return sizeof bytes_; }

private:
    std::uint8_t bytes_[DisplayInfoResponse::encoded_size];
};

}  // namespace

TEST(display_info_response, has_no_scanouts_before_it_decodes_one)
{ const DisplayInfoResponse info;
  CHECK_EQ(info.enabled_count(), 0u);
  CHECK(!info.enabled(0u));
  CHECK_EQ(info.width(0u), 0u);
  CHECK_EQ(info.height(0u), 0u);
  CHECK(info.invariants_hold()); }

TEST(display_info_response_decode, reads_the_rectangle_of_every_enabled_scanout)
{ Answer answer;
  answer.scanout(0u, 0u, 0u, 1280u, 800u);
  answer.scanout(2u, 1280u, 0u, 1024u, 768u);
  DisplayInfoResponse info;
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::ok);
  CHECK_EQ(info.enabled_count(), 2u);
  CHECK(info.enabled(0u));
  CHECK_EQ(info.width(0u), 1280u);
  CHECK_EQ(info.height(0u), 800u);
  CHECK(!info.enabled(1u));
  CHECK_EQ(info.width(1u), 0u);
  CHECK(info.enabled(2u));
  CHECK_EQ(info.width(2u), 1024u);
  CHECK_EQ(info.height(2u), 768u);
  CHECK(info.invariants_hold()); }

TEST(display_info_response_decode, answers_nothing_for_a_scanout_the_device_does_not_have)
{ Answer answer;
  answer.scanout(0u, 0u, 0u, 1280u, 800u);
  DisplayInfoResponse info;
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::ok);
  CHECK(!info.enabled(DisplayInfoResponse::max_scanouts));
  CHECK_EQ(info.width(DisplayInfoResponse::max_scanouts), 0u);
  CHECK_EQ(info.height(DisplayInfoResponse::max_scanouts), 0u); }

TEST(display_info_response_decode, refuses_an_answer_that_is_not_display_info)
{ Answer answer;
  answer.scanout(0u, 0u, 0u, 1280u, 800u);
  answer.field(0u, 0x1100u);   /* OK_NODATA */
  DisplayInfoResponse info;
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::malformed);
  answer.field(0u, 0x1200u);   /* ERR_UNSPEC */
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::malformed); }

TEST(display_info_response_decode, refuses_an_answer_truncated_at_any_field)
{ Answer answer;
  answer.scanout(0u, 0u, 0u, 1280u, 800u);
  DisplayInfoResponse info;
  for (std::size_t length = 0u; length < answer.length(); length += 4u)
      CHECK_STATUS(info.decode(answer.bytes(), length), Status::malformed);
  CHECK_STATUS(info.decode(nullptr, answer.length()), Status::malformed); }

TEST(display_info_response_decode, refuses_an_enabled_scanout_with_no_pixels)
{ Answer answer;
  answer.scanout(0u, 0u, 0u, 0u, 800u);
  DisplayInfoResponse info;
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::malformed);
  answer.scanout(0u, 0u, 0u, 1280u, 0u);
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::malformed); }

TEST(display_info_response_decode, refuses_a_rectangle_no_display_could_have)
{ Answer answer;
  const std::uint32_t too_wide = DisplayInfoResponse::max_dimension + 1u;
  answer.scanout(0u, 0u, 0u, too_wide, 800u);
  DisplayInfoResponse info;
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::malformed);
  answer.scanout(0u, 0u, 0u, 1280u, too_wide);
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::malformed);
  answer.scanout(0u, too_wide, 0u, 1280u, 800u);
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::malformed);
  answer.scanout(0u, 0u, too_wide, 1280u, 800u);
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::malformed); }

TEST(display_info_response_decode, refuses_an_enabled_flag_that_is_neither_on_nor_off)
{ Answer answer;
  answer.scanout(0u, 0u, 0u, 1280u, 800u, 2u);
  DisplayInfoResponse info;
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::malformed); }

/* A disabled scanout says nothing about its rectangle, so nothing in it is
 * judged - QEMU leaves the bytes of an unused one as they were. */
TEST(display_info_response_decode, ignores_the_rectangle_of_a_disabled_scanout)
{ Answer answer;
  answer.scanout(0u, 0u, 0u, 1280u, 800u);
  answer.scanout(1u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0u);
  DisplayInfoResponse info;
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::ok);
  CHECK_EQ(info.enabled_count(), 1u);
  CHECK(!info.enabled(1u));
  CHECK_EQ(info.width(1u), 0u); }

/* A refused answer leaves what was decoded before it, so one bad answer from
 * the host does not lose the modes the driver is offering. */
TEST(display_info_response_decode, keeps_the_last_good_answer_when_one_is_refused)
{ Answer answer;
  answer.scanout(0u, 0u, 0u, 1280u, 800u);
  DisplayInfoResponse info;
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::ok);
  Answer broken;
  broken.scanout(0u, 0u, 0u, 0u, 0u);
  CHECK_STATUS(info.decode(broken.bytes(), broken.length()), Status::malformed);
  CHECK_EQ(info.enabled_count(), 1u);
  CHECK_EQ(info.width(0u), 1280u);
  CHECK(info.invariants_hold()); }

TEST(display_info_response_decode, accepts_an_answer_longer_than_it_reads)
{ Answer answer;
  answer.scanout(0u, 0u, 0u, 1280u, 800u);
  std::uint8_t longer[DisplayInfoResponse::encoded_size + 16u] = {};
  std::memcpy(longer, answer.bytes(), answer.length());
  DisplayInfoResponse info;
  CHECK_STATUS(info.decode(longer, sizeof longer), Status::ok);
  CHECK_EQ(info.width(0u), 1280u); }

TEST(display_info_response_decode, accepts_every_scanout_at_once)
{ Answer answer;
  for (std::uint32_t index = 0u; index < DisplayInfoResponse::max_scanouts; index++)
      answer.scanout(index, 0u, 0u, 640u + index, 480u);
  DisplayInfoResponse info;
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::ok);
  CHECK_EQ(info.enabled_count(), DisplayInfoResponse::max_scanouts);
  CHECK_EQ(info.width(DisplayInfoResponse::max_scanouts - 1u), 640u + DisplayInfoResponse::max_scanouts - 1u);
  CHECK(info.invariants_hold()); }

namespace av {

/* The state the invariant guards, which decode never leaves behind. */
struct DisplayInfoResponseProbe {
    static void forget_a_width(DisplayInfoResponse &info) { info.widths_[0] = 0u; }
    static void forget_a_height(DisplayInfoResponse &info) { info.heights_[0] = 0u; }
};

}  // namespace av

TEST(display_info_response_invariants, an_enabled_scanout_with_no_width_is_detected)
{ Answer answer;
  answer.scanout(0u, 0u, 0u, 1280u, 800u);
  DisplayInfoResponse info;
  CHECK_STATUS(info.decode(answer.bytes(), answer.length()), Status::ok);
  CHECK(info.invariants_hold());
  av::DisplayInfoResponseProbe::forget_a_width(info);
  CHECK(!info.invariants_hold());
  DisplayInfoResponse other;
  CHECK_STATUS(other.decode(answer.bytes(), answer.length()), Status::ok);
  av::DisplayInfoResponseProbe::forget_a_height(other);
  CHECK(!other.invariants_hold()); }
