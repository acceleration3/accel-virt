#include "miniport/av_context/context_pages.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/guest_page_list.hpp"

#include <cstdint>

using av::ContextPages;
using av::GuestPageList;

namespace {

constexpr std::uint64_t page = GuestPageList::page_bytes;

constexpr std::uint64_t ring_frames[] = {0x10u * page, 0x11u * page, 0x40u * page};
constexpr std::uint64_t fence_frame[] = {0x20u * page};
constexpr std::uint64_t reply_frame[] = {0x30u * page};
constexpr std::uint64_t two_pages[] = {0x20u * page, 0x21u * page};
constexpr std::uint64_t unmapped[] = {0u};

class Views {
public:
    std::uint8_t       *ring() { return ring_; }
    const std::uint8_t *fence() const { return fence_; }

private:
    std::uint8_t ring_[32] = {};
    std::uint8_t fence_[32] = {};
};

}

TEST(context_pages, is_empty_and_incomplete_by_default)
{ const ContextPages pages;
  CHECK_EQ(pages.ring().count(), 0u);
  CHECK_EQ(pages.fence().count(), 0u);
  CHECK_EQ(pages.reply().count(), 0u);
  CHECK(pages.ring_view() == nullptr);
  CHECK(pages.fence_view() == nullptr);
  CHECK(!pages.complete()); }

TEST(context_pages, hands_back_the_frames_and_views_it_was_built_from)
{ Views views;
  const ContextPages pages(GuestPageList(ring_frames, 3u), GuestPageList(fence_frame, 1u),
                           GuestPageList(reply_frame, 1u), views.ring(), views.fence());
  CHECK(pages.complete());
  CHECK_EQ(pages.ring().count(), 3u);
  CHECK_EQ(pages.ring().runs(), 2u);
  CHECK(pages.ring().frame(2u) == 0x40u * page);
  CHECK(pages.fence().frame(0u) == 0x20u * page);
  CHECK(pages.reply().frame(0u) == 0x30u * page);
  CHECK(pages.ring_view() == views.ring());
  CHECK(pages.fence_view() == views.fence());
  const ContextPages copy = pages;
  CHECK(copy.complete());
  CHECK(copy.ring_view() == views.ring()); }

TEST(context_pages_complete, every_missing_or_unmapped_part_is_incomplete)
{ Views views;
  const GuestPageList ring(ring_frames, 3u);
  const GuestPageList fence(fence_frame, 1u);
  const GuestPageList reply(reply_frame, 1u);
  const GuestPageList hole(unmapped, 1u);
  CHECK(!ContextPages(hole, fence, reply, views.ring(), views.fence()).complete());
  CHECK(!ContextPages(ring, hole, reply, views.ring(), views.fence()).complete());
  CHECK(!ContextPages(ring, fence, hole, views.ring(), views.fence()).complete());
  CHECK(!ContextPages(GuestPageList(), fence, reply, views.ring(), views.fence()).complete());
  CHECK(!ContextPages(ring, GuestPageList(), reply, views.ring(), views.fence()).complete());
  CHECK(!ContextPages(ring, fence, GuestPageList(), views.ring(), views.fence()).complete());
  CHECK(!ContextPages(ring, fence, reply, nullptr, views.fence()).complete());
  CHECK(!ContextPages(ring, fence, reply, views.ring(), nullptr).complete());
  CHECK(ContextPages(ring, fence, reply, views.ring(), views.fence()).complete()); }

TEST(context_pages_complete, a_fence_of_more_than_one_page_is_incomplete)
{ Views views;
  CHECK(!ContextPages(GuestPageList(ring_frames, 3u), GuestPageList(two_pages, 2u), GuestPageList(reply_frame, 1u),
                      views.ring(), views.fence())
             .complete()); }
