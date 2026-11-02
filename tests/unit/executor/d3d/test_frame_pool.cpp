#include "executor/d3d/frame_claim.hpp"
#include "executor/d3d/frame_pool.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::FrameClaim;
using av::FramePool;

namespace {
class Frame {
public:
    Frame() : claim_(), size_(0u), busy_(false) {}
    bool claim() { return !busy_ && claim_.claim(); }
    void give_back() { claim_.give_back(); }
    bool out() const { return claim_.out(); }
    bool fits(std::uint32_t size) const { return size_ == size; }
    void hold(std::uint32_t size) { size_ = size; }
    void keep_busy(bool busy) { busy_ = busy; }

private:
    FrameClaim    claim_;
    std::uint32_t size_;
    bool          busy_;
};

using Pool = FramePool<Frame>;

}

TEST(frame_pool, a_present_takes_the_first_free_frame_when_none_is_its_shape)
{ Pool   pool;
  Frame *const first = pool.claim([](const Frame &frame) { return frame.fits(64u); });
  CHECK(first != nullptr);
  CHECK(first->out());
  Frame *const second = pool.claim([](const Frame &frame) { return frame.fits(64u); });
  CHECK(second != nullptr);
  CHECK(second != first); }

TEST(frame_pool, a_free_frame_of_the_shape_is_taken_before_the_rest)
{ Pool          pool;
  std::uint32_t index = 0u;
  Frame        *sized = nullptr;
  pool.each([&](Frame &frame) {
      if (index++ == 5u) {
          frame.hold(128u);
          sized = &frame;
      }
  });
  CHECK_EQ(index, Pool::capacity);
  CHECK(pool.claim([](const Frame &frame) { return frame.fits(128u); }) == sized);
  Frame *const next = pool.claim([](const Frame &frame) { return frame.fits(128u); });
  CHECK(next != nullptr);
  CHECK(next != sized);
  sized->give_back();
  CHECK(pool.claim([](const Frame &frame) { return frame.fits(128u); }) == sized); }

TEST(frame_pool, a_frame_that_refuses_its_claim_is_passed_over)
{ Pool   pool;
  Frame *busy = nullptr;
  pool.each([&](Frame &frame) {
      if (busy == nullptr) {
          frame.hold(32u);
          frame.keep_busy(true);
          busy = &frame;
      }
  });
  Frame *const taken = pool.claim([](const Frame &frame) { return frame.fits(32u); });
  CHECK(taken != nullptr);
  CHECK(taken != busy);
  CHECK(!busy->out()); }

TEST(frame_pool, with_every_frame_out_a_present_gets_none)
{ Pool pool;
  for (std::uint32_t frame = 0u; frame < Pool::capacity; frame++)
      CHECK(pool.claim([](const Frame &) { return false; }) != nullptr);
  CHECK(pool.claim([](const Frame &) { return true; }) == nullptr);
  std::uint32_t out = 0u;
  pool.each([&](Frame &frame) { out += frame.out() ? 1u : 0u; });
  CHECK_EQ(out, Pool::capacity); }
