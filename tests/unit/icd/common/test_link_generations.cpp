#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/link_generations.hpp"

#include <cstdint>

using av::MockPlatform;
using av::icd::LinkGenerations;

namespace {

class Generation {
public:
    explicit Generation(std::uint32_t id) : id_(id), users_(0u), lost_(false) {}

    std::uint32_t id() const { return id_; }
    std::uint32_t users() const { return users_; }
    bool          lost() const { return lost_; }
    void          use() { users_++; }
    void          leave() { users_--; }
    void          lose() { lost_ = true; }

private:
    std::uint32_t id_;
    std::uint32_t users_;
    bool          lost_;
};

}

TEST(link_generations, joins_the_current_generation_and_makes_one_when_there_is_none)
{ MockPlatform platform;
  LinkGenerations<Generation> generations(platform);
  CHECK_EQ(generations.count(), 0u);
  bool left_lost = true;
  Generation *const first = generations.join(left_lost, 7u);
  CHECK(first != nullptr);
  CHECK(!left_lost);
  CHECK_EQ(first->id(), 7u);
  Generation *const again = generations.join(left_lost, 8u);
  CHECK(again == first);
  CHECK_EQ(again->id(), 7u);
  CHECK_EQ(generations.count(), 1u);
  CHECK(generations.invariants_hold()); }

TEST(link_generations, a_lost_generation_is_left_for_its_users_and_a_new_one_is_made)
{ MockPlatform platform;
  LinkGenerations<Generation> generations(platform);
  bool left_lost = false;
  Generation *const first = generations.join(left_lost, 1u);
  first->use();
  first->lose();
  Generation *const second = generations.join(left_lost, 2u);
  CHECK(left_lost);
  CHECK(second != first);
  CHECK_EQ(second->id(), 2u);
  CHECK_EQ(generations.count(), 2u);
  std::uint32_t current = 0u;
  std::uint32_t others = 0u;
  generations.for_each([&current, &others](const Generation &held, bool is_current) {
      if (is_current) current = held.id();
      else others++;
  });
  CHECK_EQ(current, 2u);
  CHECK_EQ(others, 1u);
  first->leave();
  Generation *const third = generations.join(left_lost, 3u);
  CHECK(!left_lost);
  CHECK(third == second);
  CHECK_EQ(generations.count(), 1u);
  CHECK(generations.invariants_hold()); }

TEST(link_generations_drop, forgets_the_generation_it_is_given_and_the_next_join_makes_another)
{ MockPlatform platform;
  LinkGenerations<Generation> generations(platform);
  bool left_lost = false;
  Generation *const first = generations.join(left_lost, 1u);
  generations.drop(*first);
  CHECK_EQ(generations.count(), 0u);
  CHECK(generations.invariants_hold());
  Generation *const second = generations.join(left_lost, 2u);
  CHECK_EQ(second->id(), 2u);
  CHECK_EQ(generations.count(), 1u); }
