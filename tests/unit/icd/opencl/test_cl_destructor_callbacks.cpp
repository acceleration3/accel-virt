/* The destructor callbacks kept for objects, on their own: what is taken for
 * the objects that went, in which order a Due runs them, and what the room
 * holds. */
#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_destructor_callbacks.hpp"

#include <cstring>

using av::Status;
using av::clicd::ClDestructorCallbacks;

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

}  // namespace

TEST(cl_destructor_callbacks, only_the_objects_that_went_are_taken_and_a_due_runs_them_latest_first)
{ ClDestructorCallbacks callbacks;
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
  ClDestructorCallbacks::Due due;
  callbacks.take(gone, 2u, due);
  CHECK_EQ(due.count(), 3u);
  CHECK_EQ(callbacks.kept(), 1u);
  /* Taken is not run: the owner runs them once its lock is let go. */
  CHECK_EQ(std::strcmp(ran.order(), ""), 0);
  due.run();
  /* The buffer's two before the context, as they went; each object's
   * latest first. */
  CHECK_EQ(std::strcmp(ran.order(), "dac"), 0);
  CHECK(ran.last() == &context);
  CHECK(callbacks.invariants_hold());

  /* Nothing went: nothing is taken. */
  ClDestructorCallbacks::Due none;
  callbacks.take(gone, 0u, none);
  CHECK_EQ(none.count(), 0u);
  CHECK_EQ(callbacks.kept(), 1u); }

TEST(cl_destructor_callbacks, a_row_with_no_object_or_callback_is_refused_and_the_room_is_bounded)
{ ClDestructorCallbacks callbacks;
  int object = 0;
  CHECK(callbacks.keep(static_cast<cl_mem>(nullptr), &mem_went, nullptr) == Status::invalid_parameter);
  CHECK(callbacks.keep(reinterpret_cast<cl_context>(&object), nullptr, nullptr) == Status::invalid_parameter);
  for (unsigned index = 0u; index < ClDestructorCallbacks::max_rows; ++index)
      CHECK(callbacks.keep(reinterpret_cast<cl_mem>(&object), &mem_went, nullptr) == Status::ok);
  CHECK(callbacks.keep(reinterpret_cast<cl_mem>(&object), &mem_went, nullptr) == Status::no_memory);
  CHECK_EQ(callbacks.kept(), ClDestructorCallbacks::max_rows);
  CHECK(callbacks.invariants_hold()); }
