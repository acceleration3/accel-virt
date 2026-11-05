#include "executor/sharing/d3d_retired.hpp"
#include "executor/sharing/d3d_share_state.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

using av::CountedObject;
using av::D3dRetired;
using av::D3dShareState;
using av::MockPlatform;
using av::ShareOrigin;

namespace {

uint32_t drained(D3dRetired &retired)
{ IUnknown *objects[8] = {};
  uint32_t   total = 0u;
  for (uint32_t taken = retired.take(objects, 8u); taken != 0u; taken = retired.take(objects, 8u)) {
      for (uint32_t index = 0u; index < taken; index++) objects[index]->Release();
      total += taken;
  }
  return total; }

}

TEST(d3d_share_state, holds_nothing_until_it_is_given_a_resource)
{ D3dShareState state(ShareOrigin::d3d12);
  CHECK(state.resource() == nullptr);
  CHECK(state.view() == nullptr);
  CHECK(state.shown() == nullptr);
  CHECK_EQ(state.writes(), 0u);
  CHECK_EQ(state.opens(), 0u); }

TEST(d3d_share_state, keeps_a_reference_to_its_resource_and_the_view_falls_back_to_it)
{ CountedObject resource;
  D3dShareState state(ShareOrigin::d3d12);
  state.keep_resource(&resource);
  CHECK_EQ(resource.references(), 2u);
  CHECK(state.resource() == &resource);
  CHECK(state.view() == &resource);
  CHECK(state.shown() == nullptr); }

TEST(d3d_share_state, a_view_replaces_the_resource_as_the_view_and_the_old_view_waits_to_be_released)
{ CountedObject resource, view, newer_view;
  MockPlatform  platform;
  D3dRetired    retired(platform);
  D3dShareState state(ShareOrigin::d3d12);
  state.keep_resource(&resource);
  state.keep_view(&view, retired);
  CHECK(state.view() == &view);
  CHECK(state.resource() == &resource);
  CHECK_EQ(retired.live(), 0u);
  state.keep_view(&newer_view, retired);
  CHECK(state.view() == &newer_view);
  CHECK_EQ(retired.live(), 1u);
  CHECK_EQ(view.references(), 2u);
  CHECK_EQ(drained(retired), 1u);
  CHECK_EQ(view.references(), 1u);
  CHECK_EQ(newer_view.references(), 2u);
  CHECK_EQ(resource.references(), 2u); }

TEST(d3d_share_state, a_shown_copy_replaces_the_previous_one_and_is_what_opens)
{ CountedObject resource, view, shown, newer_shown;
  MockPlatform  platform;
  D3dRetired    retired(platform);
  D3dShareState state(ShareOrigin::d3d12);
  state.keep_resource(&resource);
  state.keep_view(&view, retired);
  bool live = true;
  IUnknown *opened = state.open(live);
  CHECK(opened == &view);
  opened->Release();
  state.keep_shown(&shown, retired);
  state.keep_shown(&newer_shown, retired);
  CHECK(state.shown() == &newer_shown);
  CHECK_EQ(retired.live(), 1u);
  opened = state.open(live);
  CHECK(opened == &newer_shown);
  opened->Release();
  CHECK_EQ(drained(retired), 1u);
  CHECK_EQ(shown.references(), 1u); }

TEST(d3d_share_state, an_open_gets_a_reference_of_its_own_and_counts)
{ CountedObject resource;
  D3dShareState state(ShareOrigin::d3d11);
  bool          live = true;
  CHECK(state.open(live) == nullptr);
  CHECK(!live);
  CHECK_EQ(state.opens(), 0u);
  state.keep_resource(&resource);
  IUnknown *const first = state.open(live);
  IUnknown *const second = state.open(live);
  CHECK(first == &resource);
  CHECK(second == &resource);
  CHECK_EQ(resource.references(), 4u);
  CHECK_EQ(state.opens(), 2u);
  first->Release();
  second->Release(); }

TEST(d3d_share_state, a_presented_share_tells_the_first_open_before_a_copy_is_shown_that_it_is_live)
{ CountedObject resource, shown;
  MockPlatform  platform;
  D3dRetired    retired(platform);
  D3dShareState state(ShareOrigin::d3d11);
  state.keep_resource(&resource);
  bool live = true;
  state.open(live)->Release();
  CHECK(!live);
  state.expect_shown();
  state.open(live)->Release();
  CHECK(live);
  state.open(live)->Release();
  CHECK(!live);
  state.keep_shown(&shown, retired);
  state.open(live)->Release();
  CHECK(!live);
  CHECK_EQ(state.opens(), 4u); }

TEST(d3d_share_state, referenced_adds_a_reference_to_what_it_is_given)
{ CountedObject object;
  D3dShareState state(ShareOrigin::d3d11);
  CHECK(state.referenced(&object) == &object);
  CHECK_EQ(object.references(), 2u);
  CHECK(state.referenced(nullptr) == nullptr);
  object.Release(); }

TEST(d3d_share_state, retire_all_hands_every_object_to_the_retired_list)
{ CountedObject resource, view, shown;
  MockPlatform  platform;
  D3dRetired    retired(platform);
  D3dShareState state(ShareOrigin::d3d12);
  state.keep_resource(&resource);
  state.keep_view(&view, retired);
  state.keep_shown(&shown, retired);
  state.retire_all(retired);
  CHECK(state.resource() == nullptr);
  CHECK(state.view() == nullptr);
  CHECK(state.shown() == nullptr);
  CHECK_EQ(retired.live(), 3u);
  CHECK_EQ(drained(retired), 3u);
  CHECK_EQ(resource.references(), 1u);
  CHECK_EQ(view.references(), 1u);
  CHECK_EQ(shown.references(), 1u);
  state.retire_all(retired);
  CHECK_EQ(retired.live(), 0u); }

TEST(d3d_share_state, a_lost_direct3d11_device_takes_its_resource_and_every_view_and_copy)
{ CountedObject resource, view, shown;
  MockPlatform  platform;
  D3dRetired    retired(platform);
  D3dShareState published11(ShareOrigin::d3d11);
  published11.keep_resource(&resource);
  published11.keep_shown(&shown, retired);
  published11.retire_direct3d11(retired);
  CHECK_EQ(retired.live(), 2u);
  CHECK(published11.resource() == nullptr);
  CHECK(published11.shown() == nullptr);
  CHECK_EQ(drained(retired), 2u);
  CountedObject resource12;
  D3dShareState published12(ShareOrigin::d3d12);
  published12.keep_resource(&resource12);
  published12.keep_view(&view, retired);
  published12.keep_shown(&shown, retired);
  published12.retire_direct3d11(retired);
  CHECK_EQ(retired.live(), 2u);
  CHECK(published12.resource() == &resource12);
  CHECK(published12.view() == &resource12);
  CHECK(published12.shown() == nullptr);
  CHECK_EQ(drained(retired), 2u);
  CountedObject object;
  D3dShareState published_object(ShareOrigin::object);
  published_object.keep_resource(&object);
  published_object.retire_direct3d11(retired);
  CHECK(published_object.resource() == &object);
  CHECK_EQ(retired.live(), 0u); }

TEST(d3d_share_state, a_lost_direct3d12_device_takes_only_the_resource_it_made)
{ CountedObject resource, view, shown;
  MockPlatform  platform;
  D3dRetired    retired(platform);
  D3dShareState published12(ShareOrigin::d3d12);
  published12.keep_resource(&resource);
  published12.keep_view(&view, retired);
  published12.keep_shown(&shown, retired);
  published12.retire_direct3d12(retired);
  CHECK_EQ(retired.live(), 1u);
  CHECK(published12.resource() == nullptr);
  CHECK(published12.view() == &view);
  CHECK(published12.shown() == &shown);
  CHECK_EQ(drained(retired), 1u);
  CHECK_EQ(resource.references(), 1u);
  published12.retire_direct3d12(retired);
  CHECK_EQ(retired.live(), 0u);
  CountedObject resource11;
  D3dShareState published11(ShareOrigin::d3d11);
  published11.keep_resource(&resource11);
  published11.retire_direct3d12(retired);
  CHECK(published11.resource() == &resource11);
  CHECK_EQ(retired.live(), 0u); }

TEST(d3d_share_state, counts_the_copies_written_into_it)
{ D3dShareState state(ShareOrigin::d3d11);
  state.written();
  state.written();
  CHECK_EQ(state.writes(), 2u); }

TEST(d3d_retired, hands_over_no_more_than_asked_and_releases_what_is_left_when_it_goes)
{ CountedObject objects[3];
  MockPlatform  platform;
  {
      D3dRetired retired(platform);
      for (CountedObject &object : objects) {
          object.AddRef();
          retired.retire(&object);
      }
      retired.retire(nullptr);
      CHECK_EQ(retired.live(), 3u);
      IUnknown *taken[2] = {};
      CHECK_EQ(retired.take(taken, 2u), 2u);
      CHECK_EQ(retired.live(), 1u);
      taken[0]->Release();
      taken[1]->Release();
      CHECK_EQ(retired.take(taken, 0u), 0u);
  }
  uint32_t released = 0u;
  for (const CountedObject &object : objects) released += object.references() == 1u ? 1u : 0u;
  CHECK_EQ(released, 3u); }

TEST(d3d_retired, an_object_it_has_no_room_for_is_released_at_once)
{ CountedObject object;
  MockPlatform  platform;
  D3dRetired    retired(platform);
  object.AddRef();
  platform.fail_allocation_at(1u);
  retired.retire(&object);
  CHECK_EQ(retired.live(), 0u);
  CHECK_EQ(object.references(), 1u); }
