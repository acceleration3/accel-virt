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
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_object_table.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Status;
using av::clicd::ClObject;
using av::clicd::ClObjectProbe;
using av::clicd::ClObjectTableProbe;

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void miscount(ObjectPool<T> &pool) { pool.live_ += 1u; }
};

}  // namespace av

namespace av::icd {

struct HandleTreeProbe {
    template <typename Node>
    static ObjectPool<Node> &pool(HandleTree<Node> &tree) { return tree.pool_; }
};

}  // namespace av::icd

namespace av::clicd {

/* The states the table never builds. An object with no dispatch table is one
 * the loader would jump through a null pointer for; one with no host handle
 * names nothing; a parent of its own kind or higher makes the chain to a
 * context unbounded; and a platform on a context is a platform the loader
 * enumerated from somewhere this ICD never opened. */
struct ClObjectTableProbe {
    /* A pool whose count disagrees with what its chunks hold: a slot leaked,
     * or one counted twice. */
    static void miscount(ClObjectTable &table) { ObjectPoolProbe::miscount(icd::HandleTreeProbe::pool(table.tree_)); }
};

struct ClObjectProbe {
    static void forget_dispatch(ClObject &object) { object.dispatch_ = nullptr; }
    static void forget_wire(ClObject &object) { object.wire_ = 0u; }
    static void adopt(ClObject &object, ClObject &parent) { object.parent_ = &parent; }
    static void parent_of_its_own_kind(ClObject &object, ClObject &parent)
    { parent.kind_ = object.kind_; object.parent_ = &parent; }
    /* A platform belongs to no context, which is the one kind that may hold
     * none - and the only way that case is ever reached. */
    static void call_it_a_platform(ClObject &object)
    { object.kind_ = ClObject::Kind::platform; object.parent_ = nullptr;
      object.context_ = nullptr; }
    static void a_platform_on_a_context(ClObject &object, GuestClContext *context)
    { object.kind_ = ClObject::Kind::platform; object.parent_ = nullptr; object.context_ = context; }
    /* A count that has saturated, which is where retain must stop and
     * release must refuse: an object whose count no longer says how many
     * must never drop to zero on a guess. */
    static void saturate(ClObject &object) { object.references_ = ~std::uint32_t{0}; }
    static void exhaust(ClObject &object) { object.references_ = 0u; }
};

}  // namespace av::clicd
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
    Table() : platform_(), trace_(), lock_(trace_, 'O'), table_(platform_, dispatch, lock_) {}

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
    MockPlatform            platform_;
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    ClObjectTable           table_;
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
  av::clicd::GuestClContext *freed = some_context;
  CHECK(rig.objects().release(*buffer, freed));
  CHECK(freed == nullptr);
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, an_object_retained_survives_as_many_releases_as_it_was_retained)
{ Table rig;
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x901u);
  buffer->retain();
  buffer->retain();
  CHECK_EQ(buffer->references(), 3u);
  av::clicd::GuestClContext *freed = nullptr;
  CHECK(rig.objects().release(*buffer, freed));
  CHECK_EQ(rig.objects().live(), 1u);
  CHECK(rig.objects().release(*buffer, freed));
  CHECK_EQ(rig.objects().live(), 1u);
  CHECK(rig.objects().release(*buffer, freed));
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
  av::clicd::GuestClContext *freed = nullptr;
  CHECK(!rig.objects().release(*buffer, freed));
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

/* The invariant catches the states the table never builds. */
TEST(cl_object_table, an_objects_invariant_catches_what_the_table_never_builds)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const queue = rig.make(ClObject::Kind::command_queue, 0x901u);
  CHECK(context != nullptr && queue != nullptr);
  CHECK(context->invariants_hold());
  CHECK(queue->invariants_hold());

  ClObjectProbe::forget_dispatch(*queue);
  CHECK(!queue->invariants_hold());

  ClObject *const second = rig.make(ClObject::Kind::program, 0x902u);
  CHECK(second != nullptr);
  ClObjectProbe::forget_wire(*second);
  CHECK(!second->invariants_hold());

  /* A parent of its own kind: the walk from an object to its context would
   * never have to end. */
  ClObject *const child = rig.make(ClObject::Kind::kernel, 0x903u);
  ClObject *const sibling = rig.make(ClObject::Kind::kernel, 0x904u);
  CHECK(child != nullptr && sibling != nullptr);
  ClObjectProbe::parent_of_its_own_kind(*child, *sibling);
  CHECK(!child->invariants_hold());

  /* A platform belongs to no context: one that names a context was made
   * somewhere this ICD never opened. */
  ClObject *const platform = rig.make(ClObject::Kind::mem, 0x905u);
  CHECK(platform != nullptr);
  ClObjectProbe::a_platform_on_a_context(*platform, context->context());
  CHECK(!platform->invariants_hold()); }

/* The application's count is this ICD's, and it has two ends. An object
 * retained more times than the count can hold no longer says how many, so it
 * never drops to zero either - it lives until its context does. An object
 * whose count already reached zero is one nothing holds, and releasing it
 * again would destroy it twice. */
TEST(cl_object_table, a_reference_count_that_cannot_say_how_many_never_reaches_zero)
{ Table rig;
  ClObject *const object = rig.make(ClObject::Kind::context, 0x900u);
  CHECK(object != nullptr);
  ClObjectProbe::saturate(*object);
  CHECK_EQ(object->references(), ~std::uint32_t{0});
  /* Retaining does not wrap. */
  object->retain();
  CHECK_EQ(object->references(), ~std::uint32_t{0});
  /* And releasing never answers "that was the last one". */
  CHECK(!object->release());
  CHECK_EQ(object->references(), ~std::uint32_t{0});
  CHECK(object->invariants_hold());

  ClObject *const spent = rig.make(ClObject::Kind::context, 0x901u);
  CHECK(spent != nullptr);
  ClObjectProbe::exhaust(*spent);
  /* Nothing holds it, so this release is not the last one either - it is a
   * release of something already gone. */
  CHECK(!spent->release());
  CHECK_EQ(spent->references(), 0u);
  CHECK(spent->invariants_hold()); }

/* The table's own invariant: the pool under it is sound, and every object in
 * it is. */
TEST(cl_object_table, the_invariant_catches_a_count_that_disagrees_with_the_pool)
{ Table rig;
  CHECK(rig.make(ClObject::Kind::context, 0x900u) != nullptr);
  CHECK(rig.objects().invariants_hold());
  ClObjectTableProbe::miscount(rig.objects());
  CHECK(!rig.objects().invariants_hold()); }

TEST(cl_object_table, the_invariant_catches_an_object_of_its_own_that_is_unsound)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const queue = rig.make(ClObject::Kind::command_queue, 0x901u);
  CHECK(context != nullptr && queue != nullptr);
  CHECK(rig.objects().invariants_hold());
  /* One object it holds is broken, and the table says so through it rather
   * than only checking its own count. */
  ClObjectProbe::forget_wire(*queue);
  CHECK(!rig.objects().invariants_hold()); }

/* The one kind that may hold no context is a platform, and the invariant has
 * to let that through: an object whose kind says it belongs to nothing is
 * sound exactly when it belongs to nothing. */
TEST(cl_object_table, a_platform_belongs_to_no_context_and_is_sound_that_way)
{ Table rig;
  ClObject *const object = rig.make(ClObject::Kind::mem, 0x6600u);
  CHECK(object != nullptr);
  ClObjectProbe::call_it_a_platform(*object);
  CHECK(object->invariants_hold()); }

/* Every parent is still held: a subtree is destroyed with its root, so a live
 * child of a dead parent cannot happen - and if it did, the table would be
 * holding an object whose chain to a context is broken. */
TEST(cl_object_table, the_invariant_catches_a_child_whose_parent_the_table_lost)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  CHECK(context != nullptr);
  ClObject *const queue = rig.make(ClObject::Kind::command_queue, 0x901u, context);
  CHECK(queue != nullptr);
  CHECK(rig.objects().invariants_hold());
  /* The parent is pointed at something the pool never held. */
  ClObject outside(dispatch, ClObject::Kind::context, 0x902u, some_context, nullptr);
  ClObjectProbe::adopt(*queue, outside);
  CHECK(!rig.objects().invariants_hold());
  ClObjectProbe::adopt(*queue, *context);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, the_nearest_of_a_kind_is_the_object_itself_or_the_closest_above_it)
{ Table table;
  ClObject *const context = table.make(ClObject::Kind::context, 0x10u);
  ClObject *const program = table.make(ClObject::Kind::program, 0x20u, context);
  ClObject *const kernel = table.make(ClObject::Kind::kernel, 0x30u, program);
  ClObject *const buffer = table.make(ClObject::Kind::mem, 0x40u, context);
  CHECK(kernel->nearest(ClObject::Kind::kernel) == kernel);
  CHECK(kernel->nearest(ClObject::Kind::program) == program);
  CHECK(kernel->nearest(ClObject::Kind::context) == context);
  CHECK(buffer->nearest(ClObject::Kind::context) == context);
  /* A buffer's parent is its context: no buffer above it. */
  CHECK(buffer->parent()->nearest(ClObject::Kind::mem) == nullptr);
  CHECK(context->nearest(ClObject::Kind::kernel) == nullptr); }

TEST(cl_object_table, a_context_let_go_of_lives_while_an_object_made_on_it_does)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const program = rig.make(ClObject::Kind::program, 0x902u, context);
  ClObject *const kernel = rig.make(ClObject::Kind::kernel, 0x903u, program);
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x905u, context);
  av::clicd::GuestClContext *freed = nullptr;
  /* What an application's wrappers do: the context and the program go first. */
  CHECK(rig.objects().release(*context, freed));
  CHECK(rig.objects().release(*program, freed));
  CHECK(freed == nullptr);
  CHECK_EQ(rig.objects().live(), 4u);
  /* Neither is the application's any more, and neither is released twice. */
  CHECK(rig.objects().made(context, ClObject::Kind::context) == nullptr);
  CHECK(rig.objects().made(program, ClObject::Kind::program) == nullptr);
  CHECK(!rig.objects().release(*context, freed));
  /* What was made on them still works. */
  CHECK(rig.objects().made(kernel, ClObject::Kind::kernel) == kernel);
  CHECK(rig.objects().made(buffer, ClObject::Kind::mem) == buffer);
  CHECK(kernel->nearest(ClObject::Kind::context) == context);
  /* The kernel takes its program with it; the buffer still holds the context. */
  CHECK(rig.objects().release(*kernel, freed));
  CHECK(freed == nullptr);
  CHECK_EQ(rig.objects().live(), 2u);
  /* The last object goes, and the context with it, which its owner is told. */
  CHECK(rig.objects().release(*buffer, freed));
  CHECK(freed == some_context);
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, a_forced_destroy_lets_go_of_what_the_object_was_made_on)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x905u, context);
  rig.objects().destroy(*buffer);
  /* The context held only by the application now: its last release frees it. */
  av::clicd::GuestClContext *freed = nullptr;
  CHECK(rig.objects().release(*context, freed));
  CHECK(freed == some_context);
  CHECK_EQ(rig.objects().live(), 0u); }
