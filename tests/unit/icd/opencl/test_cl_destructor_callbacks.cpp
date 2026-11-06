#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_destructor_callbacks.hpp"

#include <cstdint>
#include <cstring>

namespace av {
struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_capacity(GrowableArray<T> &array, std::uint32_t capacity) { array.capacity_ = capacity; }
};
}

namespace av::clicd {
struct ClDestructorCallbacksProbe {
    static void lose_the_object(ClDestructorCallbacks &callbacks) { callbacks.rows_.at(0u).object_ = nullptr; }
    static void lose_the_callback(ClDestructorCallbacks &callbacks)
    { callbacks.rows_.at(0u).mem_ = nullptr; callbacks.rows_.at(0u).context_ = nullptr; }
    static void CL_CALLBACK either_kind(cl_context, void *) {}
    static void claim_both_kinds(ClDestructorCallbacks &callbacks)
    { callbacks.rows_.at(0u).context_ = &either_kind; }
    static void break_the_row_storage(ClDestructorCallbacks &callbacks)
    { av::GrowableArrayProbe::corrupt_capacity(callbacks.rows_, 12u); }
};
}

using av::Status;
using av::clicd::ClDestructorCallbacks;
using av::clicd::ClDestructorCallbacksProbe;

namespace {

class Ran {
public:
    void note(char which, const void *object)
    {
        if (count_ < sizeof order_ - 1u) order_[count_++] = which;
        last_ = object;
    }
    const char *order() const { return order_; }
    const void *last() const { return last_; }

private:
    char        order_[8] = {};
    unsigned    count_ = 0u;
    const void *last_ = nullptr;
};

struct Tag {
    Ran *ran;
    char which;
};

void CL_CALLBACK mem_went(cl_mem object, void *user_data)
{
    const Tag &tag = *static_cast<const Tag *>(user_data);
    tag.ran->note(tag.which, object);
}

void CL_CALLBACK context_went(cl_context object, void *user_data)
{
    const Tag &tag = *static_cast<const Tag *>(user_data);
    tag.ran->note(tag.which, object);
}

}

TEST(cl_destructor_callbacks, only_the_objects_that_went_are_taken_and_a_due_runs_them_latest_first)
{ av::MockPlatform platform;
  ClDestructorCallbacks callbacks(platform);
  int  buffer = 0;
  int  other = 0;
  int  context = 0;
  Ran  ran;
  Tag  a{&ran, 'a'};
  Tag  b{&ran, 'b'};
  Tag  c{&ran, 'c'};
  Tag  d{&ran, 'd'};
  CHECK(callbacks.keep(reinterpret_cast<cl_mem>(&buffer), &mem_went, &a) == Status::ok);
  CHECK(callbacks.keep(reinterpret_cast<cl_mem>(&other), &mem_went, &b) == Status::ok);
  CHECK(callbacks.keep(reinterpret_cast<cl_context>(&context), &context_went, &c) == Status::ok);
  CHECK(callbacks.keep(reinterpret_cast<cl_mem>(&buffer), &mem_went, &d) == Status::ok);
  CHECK_EQ(callbacks.kept(), 4u);

  const void *const gone[] = {&buffer, &context};
  ClDestructorCallbacks::Due due(platform);
  callbacks.take(gone, 2u, due);
  CHECK_EQ(due.count(), 3u);
  CHECK_EQ(callbacks.kept(), 1u);
  CHECK_EQ(std::strcmp(ran.order(), ""), 0);
  due.run();
  CHECK_EQ(std::strcmp(ran.order(), "dac"), 0);
  CHECK(ran.last() == &context);
  CHECK(callbacks.invariants_hold());

  ClDestructorCallbacks::Due none(platform);
  callbacks.take(gone, 0u, none);
  CHECK_EQ(none.count(), 0u);
  CHECK_EQ(callbacks.kept(), 1u); }

TEST(cl_destructor_callbacks, a_row_with_no_object_or_callback_is_refused)
{ av::MockPlatform platform;
  ClDestructorCallbacks callbacks(platform);
  int object = 0;
  CHECK(callbacks.keep(static_cast<cl_mem>(nullptr), &mem_went, nullptr) == Status::invalid_parameter);
  CHECK(callbacks.keep(reinterpret_cast<cl_context>(&object), nullptr, nullptr) == Status::invalid_parameter);
  CHECK(callbacks.keep(reinterpret_cast<cl_mem>(&object), nullptr, nullptr) == Status::invalid_parameter);
  CHECK_EQ(callbacks.kept(), 0u);
  CHECK(callbacks.invariants_hold()); }

TEST(cl_destructor_callbacks, rows_are_kept_without_a_fixed_bound_until_memory_runs_out)
{ av::MockPlatform platform;
  ClDestructorCallbacks callbacks(platform);
  int object = 0;
  constexpr unsigned many = 1000u;
  for (unsigned index = 0u; index < many; ++index)
      CHECK(callbacks.keep(reinterpret_cast<cl_mem>(&object), &mem_went, nullptr) == Status::ok);
  CHECK_EQ(callbacks.kept(), many);
  platform.fail_allocation_at(1u);
  unsigned kept = many;
  while (callbacks.keep(reinterpret_cast<cl_mem>(&object), &mem_went, nullptr) == Status::ok) ++kept;
  CHECK(kept < 2u * many);
  CHECK_EQ(callbacks.kept(), kept);
  CHECK(callbacks.invariants_hold()); }

TEST(cl_destructor_callbacks, a_due_that_cannot_grow_counts_the_callbacks_it_lost)
{ av::MockPlatform platform;
  ClDestructorCallbacks callbacks(platform);
  int object = 0;
  Ran ran;
  Tag tag{&ran, 'a'};
  constexpr unsigned many = 5u;
  for (unsigned index = 0u; index < many; ++index)
      CHECK(callbacks.keep(reinterpret_cast<cl_mem>(&object), &mem_went, &tag) == Status::ok);
  ClDestructorCallbacks::Due due(platform);
  platform.fail_allocation_at(1u);
  const void *const gone[] = {&object};
  callbacks.take(gone, 1u, due);
  CHECK_EQ(due.lost(), 1u);
  CHECK_EQ(due.count(), many - 1u);
  CHECK_EQ(callbacks.kept(), 0u);
  due.run();
  CHECK_EQ(std::strcmp(ran.order(), "aaaa"), 0);
  CHECK(callbacks.invariants_hold()); }

TEST(cl_destructor_callbacks, the_invariant_catches_the_rows_it_is_there_to_catch)
{ const auto broken = [](void (*corrupt)(ClDestructorCallbacks &)) {
      av::MockPlatform platform;
      ClDestructorCallbacks callbacks(platform);
      int object = 0;
      CHECK(callbacks.keep(reinterpret_cast<cl_mem>(&object), &mem_went, nullptr) == Status::ok);
      CHECK(callbacks.invariants_hold());
      corrupt(callbacks);
      return callbacks.invariants_hold();
  };
  CHECK(!broken(ClDestructorCallbacksProbe::lose_the_object));
  CHECK(!broken(ClDestructorCallbacksProbe::lose_the_callback));
  CHECK(!broken(ClDestructorCallbacksProbe::claim_both_kinds));
  CHECK(!broken(ClDestructorCallbacksProbe::break_the_row_storage)); }
