#include "executor/list_hold.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <ctime>
#include <vector>

using av::CountedObject;
using Hold = av::ListHold<CountedObject, void>;

namespace {

Hold::Held held_of(CountedObject &object)
{
    return Hold::Held{&object, nullptr};
}

}

TEST(list_hold, an_object_is_held_once_however_often_it_is_added)
{ av::MockPlatform platform;
  Hold hold(platform);
  CountedObject first, second;
  bool fresh = false;
  CHECK_STATUS(hold.add(held_of(first), fresh), av::Status::ok);
  CHECK(fresh);
  CHECK_STATUS(hold.add(held_of(first), fresh), av::Status::ok);
  CHECK(!fresh);
  CHECK_STATUS(hold.add(held_of(second), fresh), av::Status::ok);
  CHECK(fresh);
  CHECK_EQ(hold.size(), 2u);
  CHECK(hold.holds(&first));
  CHECK(hold.holds(&second));
  CHECK(hold.invariants_hold()); }

TEST(list_hold, nothing_is_held_for_no_object)
{ av::MockPlatform platform;
  Hold hold(platform);
  bool fresh = true;
  CHECK_STATUS(hold.add(Hold::Held{nullptr, nullptr}, fresh), av::Status::invalid_parameter);
  CHECK(!fresh);
  CHECK_EQ(hold.size(), 0u);
  CHECK(!hold.holds(nullptr) || hold.size() == 0u);
  CHECK(hold.invariants_hold()); }

TEST(list_hold, the_last_submission_is_the_highest_mark_and_is_forgotten_on_request)
{ av::MockPlatform platform;
  Hold hold(platform);
  CHECK_EQ(hold.mark(), 0u);
  hold.submitted(7u);
  hold.submitted(3u);
  CHECK_EQ(hold.mark(), 7u);
  hold.submitted(9u);
  CHECK_EQ(hold.mark(), 9u);
  hold.forget_mark();
  CHECK_EQ(hold.mark(), 0u); }

TEST(list_hold, popping_gives_up_the_most_recently_added_object_first)
{ av::MockPlatform platform;
  Hold hold(platform);
  CountedObject first, second;
  bool fresh = false;
  CHECK_STATUS(hold.add(held_of(first), fresh), av::Status::ok);
  CHECK_STATUS(hold.add(held_of(second), fresh), av::Status::ok);
  CHECK(hold.at(1u).object_ == &second);
  hold.pop();
  CHECK(hold.holds(&first));
  CHECK(!hold.holds(&second));
  hold.pop();
  hold.pop();
  CHECK_EQ(hold.size(), 0u);
  CHECK(hold.invariants_hold()); }

TEST(list_hold, an_allocation_that_fails_holds_nothing)
{ av::MockPlatform platform;
  Hold hold(platform);
  CountedObject object;
  platform.fail_allocation_at(1u);
  bool fresh = true;
  CHECK_STATUS(hold.add(held_of(object), fresh), av::Status::no_memory);
  CHECK(!fresh);
  CHECK(!hold.holds(&object));
  CHECK(hold.invariants_hold()); }

TEST(list_hold, a_list_of_many_objects_holds_each_once_and_finds_each)
{ av::MockPlatform platform;
  Hold hold(platform);
  CountedObject objects[200];
  CountedObject stranger;
  for (CountedObject &object : objects) {
      bool fresh = false;
      CHECK_STATUS(hold.add(held_of(object), fresh), av::Status::ok);
      CHECK(fresh);
  }
  for (CountedObject &object : objects) {
      bool fresh = true;
      CHECK_STATUS(hold.add(held_of(object), fresh), av::Status::ok);
      CHECK(!fresh);
      CHECK(hold.holds(&object));
  }
  CHECK(!hold.holds(&stranger));
  CHECK_EQ(hold.size(), 200u);
  CHECK(hold.invariants_hold()); }

TEST(list_hold, popping_leaves_the_objects_still_held_findable_and_the_popped_one_gone)
{ av::MockPlatform platform;
  Hold hold(platform);
  CountedObject objects[40];
  for (CountedObject &object : objects) {
      bool fresh = false;
      CHECK_STATUS(hold.add(held_of(object), fresh), av::Status::ok);
  }
  hold.pop();
  hold.pop();
  CHECK(!hold.holds(&objects[39]));
  CHECK(!hold.holds(&objects[38]));
  CHECK(hold.holds(&objects[37]));
  CHECK(hold.holds(&objects[0]));
  bool fresh = false;
  CHECK_STATUS(hold.add(held_of(objects[39]), fresh), av::Status::ok);
  CHECK(fresh);
  CHECK(hold.holds(&objects[39]));
  CHECK(hold.holds(&objects[5]));
  CHECK(hold.invariants_hold()); }

TEST(list_hold, an_index_that_cannot_grow_leaves_the_list_findable_by_scan)
{ av::MockPlatform platform;
  Hold hold(platform);
  CountedObject objects[10];
  for (std::uint32_t index = 0u; index < 8u; index++) {
      bool fresh = false;
      CHECK_STATUS(hold.add(held_of(objects[index]), fresh), av::Status::ok);
  }
  platform.fail_allocation_at(1u);
  bool fresh = true;
  CHECK_STATUS(hold.add(held_of(objects[8]), fresh), av::Status::no_memory);
  CHECK(!fresh);
  CHECK(hold.holds(&objects[3]));
  CHECK(!hold.holds(&objects[8]));
  CHECK_STATUS(hold.add(held_of(objects[8]), fresh), av::Status::ok);
  CHECK(hold.holds(&objects[8]));
  CHECK(hold.holds(&objects[0])); }

namespace {

double seconds_now()
{
    timespec at = {};
    clock_gettime(CLOCK_MONOTONIC, &at);
    return static_cast<double>(at.tv_sec) + static_cast<double>(at.tv_nsec) / 1e9;
}

struct Pinned {
    int taken = 0;
};

}

TEST(list_hold, a_pin_is_adopted_by_the_object_that_lacks_one_through_the_index_however_long_the_list)
{ av::MockPlatform platform;
  av::ListHold<CountedObject, Pinned> hold(platform);
  constexpr std::uint32_t many = 200000u;
  std::vector<CountedObject> objects(many);
  std::vector<Pinned>        pins(many);
  bool fresh = false;
  for (std::uint32_t at = 0u; at < many; at++)
      CHECK_STATUS(hold.add(av::ListHold<CountedObject, Pinned>::Held{&objects[at], nullptr}, fresh), av::Status::ok);
  const double started = seconds_now();
  for (std::uint32_t at = 0u; at < many; at++) CHECK(hold.adopt_pin(&objects[at], &pins[at]));
  CHECK(seconds_now() - started < 3.0);
  CHECK(!hold.adopt_pin(&objects[0], &pins[1]));
  CHECK(hold.at(many - 1u).pin_ == &pins[many - 1u]);
  CHECK(hold.at(0u).pin_ == &pins[0]); }

TEST(list_hold, a_pin_is_not_adopted_for_an_object_the_list_does_not_hold_or_when_there_is_no_pin)
{ av::MockPlatform platform;
  av::ListHold<CountedObject, Pinned> hold(platform);
  CountedObject held, other;
  Pinned        pin;
  bool          fresh = false;
  CHECK(!hold.adopt_pin(&held, &pin));
  CHECK_STATUS(hold.add(av::ListHold<CountedObject, Pinned>::Held{&held, nullptr}, fresh), av::Status::ok);
  CHECK(!hold.adopt_pin(&other, &pin));
  CHECK(!hold.adopt_pin(&held, nullptr));
  CHECK(hold.adopt_pin(&held, &pin)); }

TEST(list_hold, a_removed_object_is_no_longer_held_and_the_others_stay)
{ av::MockPlatform platform;
  Hold hold(platform);
  CountedObject first, second, third;
  bool fresh = false;
  CHECK_STATUS(hold.add(held_of(first), fresh), av::Status::ok);
  CHECK_STATUS(hold.add(held_of(second), fresh), av::Status::ok);
  CHECK_STATUS(hold.add(held_of(third), fresh), av::Status::ok);
  Hold::Held removed = {};
  CHECK(hold.remove(&first, removed));
  CHECK(removed.object_ == &first);
  CHECK(!hold.remove(&first, removed));
  CHECK(!hold.holds(&first));
  CHECK(hold.holds(&second));
  CHECK(hold.holds(&third));
  CHECK_EQ(hold.size(), 2u);
  CHECK(hold.invariants_hold()); }
