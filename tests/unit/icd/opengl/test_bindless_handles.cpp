#include "harness/framework/test_registry.hpp"
#include "harness/platform/failing_blocks.hpp"
#include "icd/opengl/bindless_handles.hpp"

#include <cstdint>

using av::Status;
using av::glicd::BindlessHandles;
using Kind = BindlessHandles::Kind;

TEST(bindless_handles, a_handle_is_valid_for_the_group_and_kind_it_was_added_under)
{ av::FailingBlocks blocks;
  BindlessHandles handles(blocks);
  CHECK_STATUS(handles.add(1u, 0x40u, Kind::texture, 7u), Status::ok);
  CHECK(handles.valid(1u, 0x40u, Kind::texture));
  CHECK(!handles.valid(1u, 0x40u, Kind::image));
  CHECK(!handles.valid(2u, 0x40u, Kind::texture));
  CHECK(!handles.valid(1u, 0x41u, Kind::texture));
  CHECK(handles.invariants_hold()); }

TEST(bindless_handles, the_same_handle_value_in_two_groups_is_two_handles)
{ av::FailingBlocks blocks;
  BindlessHandles handles(blocks);
  CHECK_STATUS(handles.add(1u, 5u, Kind::texture, 1u), Status::ok);
  CHECK_STATUS(handles.add(2u, 5u, Kind::image, 1u), Status::ok);
  CHECK(handles.valid(1u, 5u, Kind::texture));
  CHECK(handles.valid(2u, 5u, Kind::image));
  CHECK(!handles.valid(1u, 5u, Kind::image));
  CHECK_EQ(handles.count(), 2u); }

TEST(bindless_handles, zero_is_no_handle)
{ av::FailingBlocks blocks;
  BindlessHandles handles(blocks);
  CHECK_STATUS(handles.add(1u, 0u, Kind::texture, 1u), Status::invalid_parameter);
  CHECK(!handles.valid(1u, 0u, Kind::texture)); }

TEST(bindless_handles, deleting_a_texture_forgets_its_handles_in_that_group_only)
{ av::FailingBlocks blocks;
  BindlessHandles handles(blocks);
  CHECK_STATUS(handles.add(1u, 10u, Kind::texture, 4u), Status::ok);
  CHECK_STATUS(handles.add(1u, 11u, Kind::image, 4u), Status::ok);
  CHECK_STATUS(handles.add(1u, 12u, Kind::texture, 5u), Status::ok);
  CHECK_STATUS(handles.add(2u, 10u, Kind::texture, 4u), Status::ok);
  const std::uint32_t names[] = {4u};
  handles.forget_textures(1u, 1, names);
  CHECK(!handles.valid(1u, 10u, Kind::texture));
  CHECK(!handles.valid(1u, 11u, Kind::image));
  CHECK(handles.valid(1u, 12u, Kind::texture));
  CHECK(handles.valid(2u, 10u, Kind::texture));
  handles.forget_textures(1u, 0, names);
  handles.forget_textures(1u, 1, nullptr);
  CHECK_EQ(handles.count(), 2u);
  CHECK(handles.invariants_hold()); }

TEST(bindless_handles, a_handle_that_cannot_be_recorded_is_refused)
{ av::FailingBlocks blocks;
  BindlessHandles handles(blocks);
  blocks.fail_from(1u);
  CHECK(handles.add(1u, 3u, Kind::texture, 1u) != Status::ok);
  CHECK(!handles.valid(1u, 3u, Kind::texture)); }
