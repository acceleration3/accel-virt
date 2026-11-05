#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/d3d/texture_upload.hpp"
#include "icd/d3d/upload_sink.hpp"
#include "proto/resources/texel_layout.hpp"
#include "proto/submission/channel_counter.hpp"

#include <cstdint>

using av::TexelLayout;
using av::d3d::TextureUpload;
using av::d3dproto::Box;

namespace {

class Recorded final : public av::d3d::UploadSink {
public:
    static constexpr std::uint32_t capacity = 32u;

    bool piece(const Box &box, const std::uint8_t *data, std::uint64_t bytes, std::uint32_t row_pitch,
               std::uint32_t slice_pitch, bool first) override
    {
        if (count_ < capacity) {
            boxes_[count_] = box;
            data_[count_] = data;
            bytes_[count_] = bytes;
            row_pitch_[count_] = row_pitch;
            slice_pitch_[count_] = slice_pitch;
            first_[count_] = first;
        }
        count_++;
        return count_ != refuse_at_;
    }

    void          refuse_at(std::uint32_t nth) { refuse_at_ = nth; }
    std::uint32_t count() const { return count_; }
    const Box    &box(std::uint32_t index) const { return boxes_[index]; }
    const std::uint8_t *data(std::uint32_t index) const { return data_[index]; }
    std::uint64_t bytes(std::uint32_t index) const { return bytes_[index]; }
    std::uint32_t row_pitch(std::uint32_t index) const { return row_pitch_[index]; }
    std::uint32_t slice_pitch(std::uint32_t index) const { return slice_pitch_[index]; }
    bool          first(std::uint32_t index) const { return first_[index]; }

private:
    Box                 boxes_[capacity] = {};
    const std::uint8_t *data_[capacity] = {};
    std::uint64_t       bytes_[capacity] = {};
    std::uint32_t       row_pitch_[capacity] = {};
    std::uint32_t       slice_pitch_[capacity] = {};
    bool                first_[capacity] = {};
    std::uint32_t       count_ = 0u;
    std::uint32_t       refuse_at_ = 0u;
};

const TexelLayout words(4u, 1u, 1u);
const TexelLayout blocks(8u, 4u, 4u);
std::uint8_t      memory[1u << 16] = {};

bool same(const Box &a, const Box &b)
{
    return a.left == b.left && a.top == b.top && a.front == b.front && a.right == b.right && a.bottom == b.bottom &&
           a.back == b.back;
}

}

TEST(texture_upload, a_box_that_fits_goes_as_one_piece)
{ av::MockTransport wire;
  const Box     box = {2u, 3u, 1u, 10u, 7u, 3u};
  TextureUpload upload(words, box, 64u, 512u, 4096u, wire);
  CHECK(upload.readable());
  CHECK_EQ(upload.bytes(), 512u + 3u * 64u + 32u);
  Recorded sink;
  CHECK(upload.send(sink, memory));
  CHECK_EQ(sink.count(), 1u);
  CHECK(same(sink.box(0u), box));
  CHECK(sink.data(0u) == memory);
  CHECK_EQ(sink.bytes(0u), upload.bytes());
  CHECK_EQ(sink.row_pitch(0u), 64u);
  CHECK_EQ(sink.slice_pitch(0u), 512u);
  CHECK(sink.first(0u));
  CHECK_EQ(wire.noted(av::ChannelCounter::split_transfers), 0u);
  CHECK_EQ(wire.noted(av::ChannelCounter::split_pieces), 0u); }

TEST(texture_upload, a_box_too_large_goes_a_slice_and_some_rows_at_a_time)
{ av::MockTransport wire;
  const Box     box = {0u, 0u, 4u, 16u, 4u, 6u};
  TextureUpload upload(words, box, 64u, 256u, 160u, wire);
  Recorded      sink;
  CHECK(upload.send(sink, memory));
  CHECK_EQ(sink.count(), 4u);
  CHECK_EQ(wire.noted(av::ChannelCounter::split_transfers), 1u);
  CHECK_EQ(wire.noted(av::ChannelCounter::split_pieces), 3u);
  const Box expected[4] = {{0u, 0u, 4u, 16u, 2u, 5u}, {0u, 2u, 4u, 16u, 4u, 5u},
                           {0u, 0u, 5u, 16u, 2u, 6u}, {0u, 2u, 5u, 16u, 4u, 6u}};
  const std::uint64_t offsets[4] = {0u, 128u, 256u, 384u};
  for (std::uint32_t index = 0u; index < 4u; index++) {
      CHECK(same(sink.box(index), expected[index]));
      CHECK(sink.data(index) == memory + offsets[index]);
      CHECK_EQ(sink.bytes(index), 128u);
      CHECK_EQ(sink.first(index), index == 0u);
  } }

TEST(texture_upload, block_rows_and_runs_land_on_whole_blocks)
{ av::MockTransport wire;
  const Box     box = {8u, 4u, 0u, 20u, 10u, 1u};
  TextureUpload upload(blocks, box, 64u, 0u, 16u, wire);
  Recorded      sink;
  CHECK(upload.send(sink, memory));
  CHECK_EQ(sink.count(), 4u);
  CHECK(same(sink.box(0u), Box{8u, 4u, 0u, 16u, 8u, 1u}));
  CHECK(same(sink.box(1u), Box{16u, 4u, 0u, 20u, 8u, 1u}));
  CHECK(sink.data(1u) == memory + 16u);
  CHECK(same(sink.box(3u), Box{16u, 8u, 0u, 20u, 10u, 1u}));
  CHECK(sink.data(3u) == memory + 64u + 16u);
  CHECK_EQ(sink.bytes(3u), 8u); }

TEST(texture_upload, a_buffer_goes_in_runs_of_bytes)
{ av::MockTransport wire;
  const Box     box = {100u, 0u, 0u, 350u, 1u, 1u};
  TextureUpload upload(TexelLayout(1u, 1u, 1u), box, 0u, 0u, 100u, wire);
  CHECK(upload.readable());
  Recorded sink;
  CHECK(upload.send(sink, memory));
  CHECK_EQ(sink.count(), 3u);
  CHECK(same(sink.box(2u), Box{300u, 0u, 0u, 350u, 1u, 1u}));
  CHECK(sink.data(2u) == memory + 200u);
  CHECK_EQ(sink.bytes(2u), 50u); }

TEST(texture_upload, a_box_its_memory_cannot_hold_is_refused_before_anything_is_sent)
{ av::MockTransport wire;
  Recorded sink;
  const TextureUpload empty(words, Box{4u, 0u, 0u, 4u, 4u, 1u}, 64u, 0u, 4096u, wire);
  CHECK_EQ(empty.bytes(), 0u);
  CHECK(!empty.readable());
  CHECK(!empty.send(sink, memory));
  const TextureUpload flat(words, Box{0u, 4u, 0u, 4u, 4u, 1u}, 64u, 0u, 4096u, wire);
  CHECK(!flat.readable());
  const TextureUpload shallow(words, Box{0u, 0u, 2u, 4u, 4u, 2u}, 64u, 0u, 4096u, wire);
  CHECK(!shallow.readable());
  const TextureUpload unknown(TexelLayout(0u, 1u, 1u), Box{0u, 0u, 0u, 4u, 4u, 1u}, 64u, 0u, 4096u, wire);
  CHECK(!unknown.readable());
  const TextureUpload crowded(words, Box{0u, 0u, 0u, 16u, 2u, 1u}, 32u, 0u, 4096u, wire);
  CHECK(!crowded.readable());
  const TextureUpload stacked(words, Box{0u, 0u, 0u, 16u, 2u, 2u}, 64u, 100u, 4096u, wire);
  CHECK(!stacked.readable());
  CHECK(!stacked.send(sink, memory));
  CHECK_EQ(sink.count(), 0u);
  const TextureUpload row(words, Box{0u, 0u, 0u, 16u, 1u, 1u}, 0u, 0u, 4096u, wire);
  CHECK(row.readable());
  const TextureUpload slices(words, Box{0u, 0u, 0u, 16u, 2u, 2u}, 64u, 128u, 4096u, wire);
  CHECK(slices.readable()); }

TEST(texture_upload, a_refused_piece_ends_the_upload)
{ av::MockTransport wire;
  Recorded whole;
  whole.refuse_at(1u);
  CHECK(!TextureUpload(words, Box{0u, 0u, 0u, 4u, 4u, 1u}, 16u, 0u, 4096u, wire).send(whole, memory));
  Recorded split;
  split.refuse_at(2u);
  CHECK(!TextureUpload(words, Box{0u, 0u, 0u, 16u, 4u, 2u}, 64u, 256u, 160u, wire).send(split, memory));
  CHECK_EQ(split.count(), 2u); }
