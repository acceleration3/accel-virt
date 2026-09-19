/* The only place a pointer from the application is believed.
 *
 * An OpenCL application passes back whatever it likes, and the Khronos loader
 * hands it straight through: a released object, one of another kind, a stack
 * address, a number it made up. None of them may be read through to find out,
 * so what is asserted here is that each is refused without being touched, and
 * that what is accepted is exactly what this table made.
 *
 * The other half is the tree. OpenCL's own counting says a context's objects
 * die with it, and a guest that releases a context while holding a buffer must
 * not be left holding one whose context is gone. */
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_object_table.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Status;
using av::clicd::ClObject;
using av::clicd::ClObjectTable;

namespace {

/* The table every object carries. Empty is enough: what is asserted is that
 * the word is written, not what is in it. */
cl_icd_dispatch dispatch = {};

/* A context pointer that is only ever compared. */
av::clicd::GuestClContext *const some_context =
    reinterpret_cast<av::clicd::GuestClContext *>(std::uintptr_t{0x1234});

class Table {
public:
    Table() : platform_(), table_(platform_, dispatch) {}

    ClObjectTable &objects() { return table_; }
    MockPlatform  &platform() { return platform_; }

    /* Makes one, and fails the test rather than returning null. */
    ClObject *make(ClObject::Kind kind, std::uint64_t wire, ClObject *parent = nullptr)
    {
        ClObject *object = nullptr;
        CHECK_STATUS(table_.make(kind, wire, some_context, parent, object), Status::ok);
        CHECK(object != nullptr);
        return object;
    }

private:
    MockPlatform  platform_;
    ClObjectTable table_;
};

}  // namespace

TEST(cl_object_table, an_empty_table_holds_nothing_and_believes_nothing)
{ Table rig;
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(rig.objects().made(nullptr, ClObject::Kind::context) == nullptr);
  int elsewhere = 0;
  CHECK(rig.objects().made(&elsewhere, ClObject::Kind::context) == nullptr);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, an_object_it_made_is_the_one_it_answers_for)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  CHECK(rig.objects().made(context, ClObject::Kind::context) == context);
  CHECK_EQ(context->wire(), 0x900u);
  CHECK_EQ(context->references(), 1u);
  CHECK_EQ(rig.objects().live(), 1u);
  CHECK(context->invariants_hold());
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, an_object_of_another_kind_is_not_that_object)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  /* The loader dispatches by table, not by type, so an application may pass a
   * cl_context where a cl_mem belongs and nothing stops it earlier. */
  CHECK(rig.objects().made(context, ClObject::Kind::mem) == nullptr);
  CHECK(rig.objects().made(context, ClObject::Kind::command_queue) == nullptr);
  CHECK(rig.objects().made(context, ClObject::Kind::context) == context); }

TEST(cl_object_table, an_address_that_is_not_an_object_is_refused_without_being_read)
{ Table rig;
  rig.make(ClObject::Kind::context, 0x900u);
  /* A stack address, and one just past a real object: neither is dereferenced
   * to find out, which is the whole point. */
  std::uint8_t stack[64] = {};
  CHECK(rig.objects().made(stack, ClObject::Kind::context) == nullptr);
  CHECK(rig.objects().made(stack + 8, ClObject::Kind::mem) == nullptr);
  CHECK(rig.objects().made(reinterpret_cast<void *>(std::uintptr_t{0x8}),
                           ClObject::Kind::context) == nullptr); }

TEST(cl_object_table, a_wire_handle_of_zero_names_no_object)
{ Table rig;
  ClObject *object = nullptr;
  /* Zero is what the host answers when it made nothing. */
  CHECK_STATUS(rig.objects().make(ClObject::Kind::mem, 0u, some_context, nullptr, object),
               Status::invalid_parameter);
  CHECK(object == nullptr);
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, a_parent_must_be_of_a_lower_kind_so_the_chain_is_finite)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x901u, context);
  CHECK(buffer->parent() == context);
  CHECK(buffer->descends_from(*context));
  CHECK(!context->descends_from(*buffer));

  ClObject *refused = nullptr;
  /* A context under a buffer, and a buffer under a buffer: both would make a
   * chain that need not end. */
  CHECK_STATUS(rig.objects().make(ClObject::Kind::context, 0x902u, some_context, buffer, refused),
               Status::invalid_parameter);
  CHECK_STATUS(rig.objects().make(ClObject::Kind::mem, 0x903u, some_context, buffer, refused),
               Status::invalid_parameter);
  CHECK(refused == nullptr);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, a_parent_it_does_not_hold_is_refused)
{ Table rig;
  ClObject *refused = nullptr;
  std::uint8_t stack[64] = {};
  CHECK_STATUS(rig.objects().make(ClObject::Kind::mem, 0x900u, some_context,
                                  reinterpret_cast<ClObject *>(stack), refused),
               Status::invalid_parameter);
  CHECK(refused == nullptr);
  CHECK_EQ(rig.objects().live(), 0u); }

TEST(cl_object_table, releasing_the_last_reference_destroys_the_object)
{ Table rig;
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x901u);
  CHECK(rig.objects().release(*buffer));
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, an_object_retained_survives_as_many_releases_as_it_was_retained)
{ Table rig;
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x901u);
  buffer->retain();
  buffer->retain();
  CHECK_EQ(buffer->references(), 3u);
  CHECK(rig.objects().release(*buffer));
  CHECK_EQ(rig.objects().live(), 1u);
  CHECK(rig.objects().release(*buffer));
  CHECK_EQ(rig.objects().live(), 1u);
  CHECK(rig.objects().release(*buffer));
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, destroying_a_context_takes_its_whole_tree_at_any_depth)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const queue = rig.make(ClObject::Kind::command_queue, 0x901u, context);
  ClObject *const program = rig.make(ClObject::Kind::program, 0x902u, context);
  ClObject *const kernel = rig.make(ClObject::Kind::kernel, 0x903u, program);
  ClObject *const event = rig.make(ClObject::Kind::event, 0x904u, queue);
  rig.make(ClObject::Kind::mem, 0x905u, context);
  CHECK_EQ(rig.objects().live(), 6u);
  CHECK(kernel->descends_from(*context));
  CHECK(event->descends_from(*context));

  rig.objects().destroy(*context);
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, destroying_one_branch_leaves_the_others)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const program = rig.make(ClObject::Kind::program, 0x902u, context);
  rig.make(ClObject::Kind::kernel, 0x903u, program);
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x905u, context);
  CHECK_EQ(rig.objects().live(), 4u);

  rig.objects().destroy(*program);
  CHECK_EQ(rig.objects().live(), 2u);
  CHECK(rig.objects().made(buffer, ClObject::Kind::mem) == buffer);
  CHECK(rig.objects().made(context, ClObject::Kind::context) == context);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, a_destroyed_object_is_no_longer_one_it_answers_for)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x901u, context);
  rig.objects().destroy(*context);
  /* The application still holds both pointers. Neither is an object now, and
   * neither is read to say so. */
  CHECK(rig.objects().made(context, ClObject::Kind::context) == nullptr);
  CHECK(rig.objects().made(buffer, ClObject::Kind::mem) == nullptr);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, releasing_or_destroying_what_it_does_not_hold_changes_nothing)
{ Table rig;
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x901u);
  rig.objects().destroy(*buffer);
  CHECK_EQ(rig.objects().live(), 0u);
  /* The application releases it again, as applications do. */
  CHECK(!rig.objects().release(*buffer));
  rig.objects().destroy(*buffer);
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, counts_by_kind_are_what_a_state_dump_reports)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  rig.make(ClObject::Kind::command_queue, 0x901u, context);
  rig.make(ClObject::Kind::mem, 0x902u, context);
  rig.make(ClObject::Kind::mem, 0x903u, context);
  CHECK_EQ(rig.objects().live_of(ClObject::Kind::mem), 2u);
  CHECK_EQ(rig.objects().live_of(ClObject::Kind::command_queue), 1u);
  CHECK_EQ(rig.objects().live_of(ClObject::Kind::kernel), 0u);
  CHECK_EQ(rig.objects().live(), 4u); }

TEST(cl_object_table, a_table_out_of_memory_makes_nothing_and_stays_sound)
{ Table rig;
  rig.platform().fail_allocation_at(1u);
  ClObject *object = nullptr;
  CHECK_STATUS(rig.objects().make(ClObject::Kind::context, 0x900u, some_context, nullptr, object),
               Status::no_memory);
  CHECK(object == nullptr);
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, every_object_carries_the_dispatch_table_the_loader_reads)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  /* The ICD ABI is the first word of the object: the loader reads it and
   * jumps, checking nothing. */
  const void *const first_word = *reinterpret_cast<void *const *>(context);
  CHECK(first_word == static_cast<const void *>(&dispatch)); }

/* Where a buffer's memory is, when it is the context's blob arena.
 *
 * Only a mem ever carries one, and only when the arena had room; everything
 * else, and a staged buffer, answers that it is backed by nothing - which is
 * what sends a map down the staging path. */
TEST(cl_object_table, only_a_buffer_given_arena_memory_says_it_is_backed)
{ Table rig;
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x6600u);
  CHECK(buffer != nullptr);
  /* Made backed by nothing: a buffer the arena had no room for takes the
   * staged path, and so does every object that is not a buffer at all. */
  CHECK(!buffer->backed());
  CHECK_EQ(buffer->backing_offset(), 0u);
  CHECK_EQ(buffer->backing_bytes(), 0u);
  CHECK(buffer->invariants_hold());

  buffer->back_with(0x2000u, 256u);
  CHECK(buffer->backed());
  CHECK_EQ(buffer->backing_offset(), 0x2000u);
  CHECK_EQ(buffer->backing_bytes(), 256u);
  CHECK(buffer->invariants_hold());

  /* Offset zero is where the first block of the arena sits, so a length is
   * what says a buffer is backed and never the offset. */
  ClObject *const first = rig.make(ClObject::Kind::mem, 0x6601u);
  CHECK(first != nullptr);
  first->back_with(0u, 4096u);
  CHECK(first->backed());
  CHECK_EQ(first->backing_offset(), 0u);
  CHECK(first->invariants_hold());

  /* Given back when the buffer is released, which leaves it naming nothing
   * again - the block may be another buffer's by then. */
  first->back_with(0u, 0u);
  CHECK(!first->backed());
  CHECK(first->invariants_hold()); }
