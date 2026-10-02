#include "miniport/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "miniport/memory/aperture_pages.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/guest_page_list.hpp"

#include <cstdint>

using av::AperturePages;
using av::DxgkBuildPagingBufferMapApertureSegmentRecord;
using av::DxgkBuildPagingBufferUnmapApertureSegmentRecord;
using av::GuestPageList;
using av::MockPlatform;
using av::Status;

namespace {

constexpr std::uint64_t page = GuestPageList::page_bytes;

DxgkBuildPagingBufferMapApertureSegmentRecord mapping(void *mdl, std::uint64_t offset, std::uint64_t pages)
{
    DxgkBuildPagingBufferMapApertureSegmentRecord request = {};
    request.SegmentId = 1u;
    request.OffsetInPages = offset;
    request.NumberOfPages = pages;
    request.pMdl = mdl;
    return request;
}

DxgkBuildPagingBufferUnmapApertureSegmentRecord unmapping(std::uint64_t offset, std::uint64_t pages)
{
    DxgkBuildPagingBufferUnmapApertureSegmentRecord request = {};
    request.SegmentId = 1u;
    request.OffsetInPages = offset;
    request.NumberOfPages = pages;
    return request;
}

}  // namespace

TEST(aperture_pages, has_no_pages_before_a_mapping)
{ MockPlatform platform;
  AperturePages pages(platform, 4u);
  CHECK_EQ(pages.pages(0u, 4u).count(), 0u);
  CHECK(pages.invariants_hold()); }

TEST(aperture_pages, records_the_frames_the_mdl_names)
{ MockPlatform platform;
  const std::uint64_t frames[] = {7u * page, 8u * page, 2u * page};
  void *mdl = platform.attach_page_frames(frames, 3u);
  AperturePages pages(platform, 3u);
  CHECK_STATUS(pages.map(mapping(mdl, 0x40u, 3u)), Status::ok);
  const GuestPageList list = pages.pages(0x40u, 3u);
  CHECK_EQ(list.count(), 3u);
  CHECK_EQ(list.frame(0u), 7u * page);
  CHECK_EQ(list.frame(2u), 2u * page);
  CHECK_EQ(list.runs(), 2u); }

TEST(aperture_pages, records_only_the_window_the_mdl_offset_names)
{ MockPlatform platform;
  const std::uint64_t frames[] = {page, 2u * page, 3u * page, 4u * page};
  void *mdl = platform.attach_page_frames(frames, 4u);
  AperturePages pages(platform, 4u);
  DxgkBuildPagingBufferMapApertureSegmentRecord request = mapping(mdl, 0x10u, 2u);
  request.MdlOffset = 2u;
  CHECK_STATUS(pages.map(request), Status::ok);
  CHECK_EQ(pages.pages(0x10u, 2u).frame(0u), 3u * page); }

TEST(aperture_pages, answers_no_pages_for_a_window_it_never_mapped)
{ MockPlatform platform;
  const std::uint64_t frames[] = {page, 2u * page};
  void *mdl = platform.attach_page_frames(frames, 2u);
  AperturePages pages(platform, 2u);
  CHECK_STATUS(pages.map(mapping(mdl, 0x80u, 2u)), Status::ok);
  CHECK_EQ(pages.pages(0x7Fu, 2u).count(), 0u);
  CHECK_EQ(pages.pages(0x81u, 2u).count(), 0u);
  CHECK_EQ(pages.pages(0x80u, 0u).count(), 0u);
  CHECK_EQ(pages.pages(0x80u, 3u).count(), 0u);
  CHECK_EQ(pages.pages(0x81u, 1u).count(), 1u); }

TEST(aperture_pages, refuses_a_mapping_with_no_mdl)
{ MockPlatform platform;
  AperturePages pages(platform, 2u);
  CHECK_STATUS(pages.map(mapping(nullptr, 0u, 2u)), Status::invalid_parameter);
  CHECK_EQ(pages.pages(0u, 2u).count(), 0u); }

TEST(aperture_pages, refuses_a_mapping_of_no_pages)
{ MockPlatform platform;
  const std::uint64_t frames[] = {page};
  void *mdl = platform.attach_page_frames(frames, 1u);
  AperturePages pages(platform, 2u);
  CHECK_STATUS(pages.map(mapping(mdl, 0u, 0u)), Status::invalid_parameter); }

TEST(aperture_pages, refuses_more_pages_than_the_allocation_has)
{ MockPlatform platform;
  const std::uint64_t frames[] = {page, 2u * page, 3u * page};
  void *mdl = platform.attach_page_frames(frames, 3u);
  AperturePages pages(platform, 2u);
  CHECK_STATUS(pages.map(mapping(mdl, 0u, 3u)), Status::invalid_parameter); }

TEST(aperture_pages, refuses_a_window_whose_end_would_wrap)
{ MockPlatform platform;
  const std::uint64_t frames[] = {page, 2u * page};
  void *mdl = platform.attach_page_frames(frames, 2u);
  AperturePages pages(platform, 2u);
  CHECK_STATUS(pages.map(mapping(mdl, ~std::uint64_t{0} - 1u, 2u)), Status::invalid_parameter); }

TEST(aperture_pages, refuses_a_mapping_the_mdl_does_not_cover)
{ MockPlatform platform;
  const std::uint64_t frames[] = {page, 2u * page};
  void *mdl = platform.attach_page_frames(frames, 2u);
  AperturePages pages(platform, 4u);
  DxgkBuildPagingBufferMapApertureSegmentRecord request = mapping(mdl, 0u, 4u);
  CHECK_STATUS(pages.map(request), Status::invalid_parameter);
  request.NumberOfPages = 2u;
  request.MdlOffset = 1u;
  CHECK_STATUS(pages.map(request), Status::invalid_parameter);
  CHECK_EQ(pages.pages(0u, 2u).count(), 0u); }

TEST(aperture_pages, refuses_a_mapping_when_there_is_no_memory_for_the_frames)
{ MockPlatform platform;
  const std::uint64_t frames[] = {page};
  void *mdl = platform.attach_page_frames(frames, 1u);
  AperturePages pages(platform, 1u);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(pages.map(mapping(mdl, 0u, 1u)), Status::no_memory);
  CHECK_STATUS(pages.map(mapping(mdl, 0u, 1u)), Status::ok); }

TEST(aperture_pages, forgets_the_pages_an_unmapping_takes_back)
{ MockPlatform platform;
  const std::uint64_t frames[] = {page, 2u * page, 3u * page};
  void *mdl = platform.attach_page_frames(frames, 3u);
  AperturePages pages(platform, 3u);
  CHECK_STATUS(pages.map(mapping(mdl, 0x20u, 3u)), Status::ok);
  pages.unmap(unmapping(0x21u, 1u));
  CHECK_EQ(pages.pages(0x20u, 3u).count(), 0u);
  CHECK_EQ(pages.pages(0x20u, 1u).frame(0u), page);
  CHECK_EQ(pages.pages(0x21u, 1u).count(), 0u);
  CHECK(pages.invariants_hold()); }

TEST(aperture_pages, keeps_the_pages_an_unmapping_elsewhere_leaves_alone)
{ MockPlatform platform;
  const std::uint64_t frames[] = {page, 2u * page};
  void *mdl = platform.attach_page_frames(frames, 2u);
  AperturePages pages(platform, 2u);
  CHECK_STATUS(pages.map(mapping(mdl, 0x20u, 2u)), Status::ok);
  pages.unmap(unmapping(0x30u, 2u));
  pages.unmap(unmapping(0x10u, 2u));
  pages.unmap(unmapping(0x20u, 0u));
  pages.unmap(unmapping(~std::uint64_t{0} - 1u, 4u));
  CHECK_EQ(pages.pages(0x20u, 2u).count(), 2u); }

TEST(aperture_pages, forgets_nothing_before_the_first_mapping)
{ MockPlatform platform;
  AperturePages pages(platform, 2u);
  pages.unmap(unmapping(0u, 2u));
  CHECK(pages.invariants_hold()); }

TEST(aperture_pages, rebases_when_the_next_mapping_lands_outside_the_window)
{ MockPlatform platform;
  const std::uint64_t frames[] = {page, 2u * page};
  void *mdl = platform.attach_page_frames(frames, 2u);
  AperturePages pages(platform, 2u);
  CHECK_STATUS(pages.map(mapping(mdl, 0x20u, 2u)), Status::ok);
  CHECK_STATUS(pages.map(mapping(mdl, 0x80u, 2u)), Status::ok);
  CHECK_EQ(pages.pages(0x20u, 2u).count(), 0u);
  CHECK_EQ(pages.pages(0x80u, 2u).count(), 2u);
  CHECK_STATUS(pages.map(mapping(mdl, 0x10u, 2u)), Status::ok);
  CHECK_EQ(pages.pages(0x80u, 2u).count(), 0u);
  CHECK_EQ(pages.pages(0x10u, 2u).count(), 2u); }

TEST(aperture_pages, fills_a_second_window_of_the_same_mapping)
{ MockPlatform platform;
  const std::uint64_t frames[] = {page, 2u * page};
  void *mdl = platform.attach_page_frames(frames, 2u);
  AperturePages pages(platform, 4u);
  CHECK_STATUS(pages.map(mapping(mdl, 0x40u, 2u)), Status::ok);
  CHECK_STATUS(pages.map(mapping(mdl, 0x42u, 2u)), Status::ok);
  const GuestPageList list = pages.pages(0x40u, 4u);
  CHECK_EQ(list.count(), 4u);
  CHECK_EQ(list.frame(3u), 2u * page);
  CHECK_EQ(list.runs(), 2u); }

namespace av {

/* The states the invariant guards, which correct code never reaches. */
struct AperturePagesProbe {
    static void forget_capacity(AperturePages &pages) { pages.capacity_ = 0u; }
    static void move_the_window(AperturePages &pages) { pages.base_page_ = 9u; }
};

}  // namespace av

TEST(aperture_pages_invariants, no_capacity_is_detected)
{ MockPlatform platform;
  AperturePages pages(platform, 2u);
  CHECK(pages.invariants_hold());
  av::AperturePagesProbe::forget_capacity(pages);
  CHECK(!pages.invariants_hold()); }

/* A window without frames describes pages that were never recorded. */
TEST(aperture_pages_invariants, a_window_with_no_frames_is_detected)
{ MockPlatform platform;
  AperturePages pages(platform, 2u);
  av::AperturePagesProbe::move_the_window(pages);
  CHECK(!pages.invariants_hold()); }
