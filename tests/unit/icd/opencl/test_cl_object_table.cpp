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

}

namespace av::icd {

struct HandleTreeProbe {
    template <typename Node>
    static ObjectPool<Node> &pool(HandleTree<Node> &tree) { return tree.pool_; }
};

}

namespace av::clicd {
struct ClObjectTableProbe {
    static void miscount(ClObjectTable &table) { ObjectPoolProbe::miscount(icd::HandleTreeProbe::pool(table.tree_)); }
};

struct ClObjectProbe {
    static void forget_dispatch(ClObject &object) { object.dispatch_ = nullptr; }
    static void forget_wire(ClObject &object) { object.wire_ = 0u; }
    static void adopt(ClObject &object, ClObject &parent) { object.parent_ = &parent; }
    static void parent_of_its_own_kind(ClObject &object, ClObject &parent)
    { parent.kind_ = object.kind_; object.parent_ = &parent; }
    static void call_it_a_platform(ClObject &object)
    { object.kind_ = ClObject::Kind::platform; object.parent_ = nullptr;
      object.context_ = nullptr; }
    static void a_platform_on_a_context(ClObject &object, GuestClContext *context)
    { object.kind_ = ClObject::Kind::platform; object.parent_ = nullptr; object.context_ = context; }
    static void saturate(ClObject &object) { object.references_ = ~std::uint32_t{0}; }
    static void exhaust(ClObject &object) { object.references_ = 0u; }
};

}
using av::clicd::ClObjectTable;

namespace {
cl_icd_dispatch dispatch = {};

av::clicd::GuestClContext *const some_context =
    reinterpret_cast<av::clicd::GuestClContext *>(std::uintptr_t{0x1234});

class Table {
public:
    Table() : platform_(), trace_(), lock_(trace_, 'O'), table_(platform_, dispatch, lock_) {}

    ClObjectTable &objects() { return table_; }
    MockPlatform  &platform() { return platform_; }

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

}

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

TEST(cl_object, a_queue_is_known_idle_until_work_is_sent_and_again_once_it_is_done)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const queue = rig.make(ClObject::Kind::command_queue, 0x901u, context);
  CHECK(queue->known_idle());
  queue->busy();
  CHECK(!queue->known_idle());
  queue->busy();
  CHECK(!queue->known_idle());
  queue->idle();
  CHECK(queue->known_idle());
  CHECK(queue->invariants_hold()); }

TEST(cl_object, an_object_keeps_the_notes_the_icd_makes_about_it)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const memory = rig.make(ClObject::Kind::mem, 0x902u, context);
  ClObject *const image = rig.make(ClObject::Kind::mem, 0x903u, context);
  ClObject *const kernel = rig.make(ClObject::Kind::kernel, 0x904u, context);
  CHECK_EQ(memory->flags(), 0u);
  memory->keep_flags(CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS);
  CHECK_EQ(memory->flags(), static_cast<std::uint64_t>(CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS));
  CHECK(image->companion() == nullptr);
  image->keep_companion(memory);
  CHECK(image->companion() == memory);
  CHECK_EQ(image->command_type(), 0u);
  image->keep_command_type(CL_COMMAND_READ_BUFFER);
  CHECK_EQ(image->command_type(), static_cast<std::uint32_t>(CL_COMMAND_READ_BUFFER));
  CHECK_EQ(kernel->traits(), 0u);
  kernel->keep_traits(ClObject::kernel_unshaped);
  CHECK_EQ(kernel->traits(), ClObject::kernel_unshaped);
  CHECK(kernel->shape() == nullptr);
  kernel->keep_shape(nullptr);
  CHECK(kernel->invariants_hold()); }

TEST(cl_object_table, an_object_of_another_kind_is_not_that_object)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  CHECK(rig.objects().made(context, ClObject::Kind::mem) == nullptr);
  CHECK(rig.objects().made(context, ClObject::Kind::command_queue) == nullptr);
  CHECK(rig.objects().made(context, ClObject::Kind::context) == context); }

TEST(cl_object_table, an_address_that_is_not_an_object_is_refused_without_being_read)
{ Table rig;
  rig.make(ClObject::Kind::context, 0x900u);
  std::uint8_t stack[64] = {};
  CHECK(rig.objects().made(stack, ClObject::Kind::context) == nullptr);
  CHECK(rig.objects().made(stack + 8, ClObject::Kind::mem) == nullptr);
  CHECK(rig.objects().made(reinterpret_cast<void *>(std::uintptr_t{0x8}),
                           ClObject::Kind::context) == nullptr); }

TEST(cl_object_table, a_wire_handle_of_zero_names_no_object)
{ Table rig;
  ClObject *object = nullptr;
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
  CHECK(buffer->nearest(ClObject::Kind::context) == context);
  CHECK(context->nearest(ClObject::Kind::mem) == nullptr);

  ClObject *refused = nullptr;
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
  CHECK(kernel->nearest(ClObject::Kind::context) == context);
  CHECK(event->nearest(ClObject::Kind::context) == context);

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
  CHECK(rig.objects().made(context, ClObject::Kind::context) == nullptr);
  CHECK(rig.objects().made(buffer, ClObject::Kind::mem) == nullptr);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, releasing_or_destroying_what_it_does_not_hold_changes_nothing)
{ Table rig;
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x901u);
  rig.objects().destroy(*buffer);
  CHECK_EQ(rig.objects().live(), 0u);
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
  const void *const first_word = *reinterpret_cast<void *const *>(context);
  CHECK(first_word == static_cast<const void *>(&dispatch)); }

TEST(cl_object_table, only_a_buffer_given_arena_memory_says_it_is_backed)
{ Table rig;
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x6600u);
  CHECK(buffer != nullptr);
  CHECK(!buffer->backed());
  CHECK_EQ(buffer->backing_offset(), 0u);
  CHECK_EQ(buffer->backing_bytes(), 0u);
  CHECK(buffer->invariants_hold());

  buffer->back_with(0x2000u, 256u);
  CHECK(buffer->backed());
  CHECK_EQ(buffer->backing_offset(), 0x2000u);
  CHECK_EQ(buffer->backing_bytes(), 256u);
  CHECK(buffer->invariants_hold());

  ClObject *const first = rig.make(ClObject::Kind::mem, 0x6601u);
  CHECK(first != nullptr);
  first->back_with(0u, 4096u);
  CHECK(first->backed());
  CHECK_EQ(first->backing_offset(), 0u);
  CHECK(first->invariants_hold());

  first->back_with(0u, 0u);
  CHECK(!first->backed());
  CHECK(first->invariants_hold()); }

TEST(cl_object_table, only_a_buffer_made_over_application_memory_names_it)
{ Table rig;
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x6600u);
  ClObject *const queue = rig.make(ClObject::Kind::command_queue, 0x6601u);
  CHECK(buffer != nullptr && queue != nullptr);
  CHECK(buffer->application_memory() == nullptr);
  CHECK(queue->application_memory() == nullptr);
  static unsigned char own[64];
  buffer->use_application_memory(own);
  CHECK(buffer->application_memory() == own);
  CHECK(!buffer->backed());
  CHECK(buffer->invariants_hold()); }

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

  ClObject *const child = rig.make(ClObject::Kind::kernel, 0x903u);
  ClObject *const sibling = rig.make(ClObject::Kind::kernel, 0x904u);
  CHECK(child != nullptr && sibling != nullptr);
  ClObjectProbe::parent_of_its_own_kind(*child, *sibling);
  CHECK(!child->invariants_hold());

  ClObject *const platform = rig.make(ClObject::Kind::mem, 0x905u);
  CHECK(platform != nullptr);
  ClObjectProbe::a_platform_on_a_context(*platform, context->context());
  CHECK(!platform->invariants_hold()); }

TEST(cl_object_table, a_reference_count_that_cannot_say_how_many_never_reaches_zero)
{ Table rig;
  ClObject *const object = rig.make(ClObject::Kind::context, 0x900u);
  CHECK(object != nullptr);
  ClObjectProbe::saturate(*object);
  CHECK_EQ(object->references(), ~std::uint32_t{0});
  object->retain();
  CHECK_EQ(object->references(), ~std::uint32_t{0});
  CHECK(!object->release());
  CHECK_EQ(object->references(), ~std::uint32_t{0});
  CHECK(object->invariants_hold());

  ClObject *const spent = rig.make(ClObject::Kind::context, 0x901u);
  CHECK(spent != nullptr);
  ClObjectProbe::exhaust(*spent);
  CHECK(!spent->release());
  CHECK_EQ(spent->references(), 0u);
  CHECK(spent->invariants_hold()); }

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
  ClObjectProbe::forget_wire(*queue);
  CHECK(!rig.objects().invariants_hold()); }

TEST(cl_object_table, a_platform_belongs_to_no_context_and_is_sound_that_way)
{ Table rig;
  ClObject *const object = rig.make(ClObject::Kind::mem, 0x6600u);
  CHECK(object != nullptr);
  ClObjectProbe::call_it_a_platform(*object);
  CHECK(object->invariants_hold()); }

TEST(cl_object_table, the_invariant_catches_a_child_whose_parent_the_table_lost)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  CHECK(context != nullptr);
  ClObject *const queue = rig.make(ClObject::Kind::command_queue, 0x901u, context);
  CHECK(queue != nullptr);
  CHECK(rig.objects().invariants_hold());
  ClObject outside(dispatch, rig.platform(), ClObject::Kind::context, 0x902u, some_context, nullptr);
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
  CHECK(buffer->parent()->nearest(ClObject::Kind::mem) == nullptr);
  CHECK(context->nearest(ClObject::Kind::kernel) == nullptr); }

TEST(cl_object_table, a_context_let_go_of_lives_while_an_object_made_on_it_does)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const program = rig.make(ClObject::Kind::program, 0x902u, context);
  ClObject *const kernel = rig.make(ClObject::Kind::kernel, 0x903u, program);
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x905u, context);
  av::clicd::GuestClContext *freed = nullptr;
  CHECK(rig.objects().release(*context, freed));
  CHECK(rig.objects().release(*program, freed));
  CHECK(freed == nullptr);
  CHECK_EQ(rig.objects().live(), 4u);
  CHECK(rig.objects().made(context, ClObject::Kind::context) == nullptr);
  CHECK(rig.objects().made(program, ClObject::Kind::program) == nullptr);
  CHECK(!rig.objects().release(*context, freed));
  CHECK(rig.objects().made(kernel, ClObject::Kind::kernel) == kernel);
  CHECK(rig.objects().made(buffer, ClObject::Kind::mem) == buffer);
  CHECK(kernel->nearest(ClObject::Kind::context) == context);
  CHECK(rig.objects().release(*kernel, freed));
  CHECK(freed == nullptr);
  CHECK_EQ(rig.objects().live(), 2u);
  CHECK(rig.objects().release(*buffer, freed));
  CHECK(freed == some_context);
  CHECK_EQ(rig.objects().live(), 0u);
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object_table, a_forced_destroy_lets_go_of_what_the_object_was_made_on)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x905u, context);
  rig.objects().destroy(*buffer);
  av::clicd::GuestClContext *freed = nullptr;
  CHECK(rig.objects().release(*context, freed));
  CHECK(freed == some_context);
  CHECK_EQ(rig.objects().live(), 0u); }

TEST(cl_object_table, a_sub_buffer_names_the_buffer_it_is_a_region_of)
{ Table rig;
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x6600u);
  ClObject *const sub = rig.make(ClObject::Kind::mem, 0x6601u);
  ClObject *const queue = rig.make(ClObject::Kind::command_queue, 0x6602u);
  CHECK(buffer != nullptr && sub != nullptr && queue != nullptr);
  CHECK(sub->sliced_from() == nullptr);
  CHECK(queue->sliced_from() == nullptr);
  sub->slice_of(*buffer);
  CHECK(sub->sliced_from() == buffer);
  CHECK(buffer->sliced_from() == nullptr);
  CHECK(sub->invariants_hold());
  buffer->slice_of(*buffer);
  CHECK(!buffer->invariants_hold());
  queue->slice_of(*sub);
  CHECK(!queue->invariants_hold()); }

TEST(cl_object_table, a_contexts_queues_are_idle_when_none_of_them_has_work_in_flight)
{ Table rig;
  ClObject *const context = rig.make(ClObject::Kind::context, 0x900u);
  ClObject *const other = rig.make(ClObject::Kind::context, 0x910u);
  CHECK(rig.objects().queues_idle(nullptr));
  CHECK(rig.objects().queues_idle(context));
  ClObject *const first = rig.make(ClObject::Kind::command_queue, 0x901u, context);
  ClObject *const second = rig.make(ClObject::Kind::command_queue, 0x902u, context);
  ClObject *const elsewhere = rig.make(ClObject::Kind::command_queue, 0x911u, other);
  ClObject *const buffer = rig.make(ClObject::Kind::mem, 0x903u, context);
  CHECK(buffer != nullptr);
  CHECK(rig.objects().queues_idle(context));
  first->busy();
  CHECK(!rig.objects().queues_idle(context));
  CHECK(rig.objects().queues_idle(other));
  first->idle();
  second->busy();
  CHECK(!rig.objects().queues_idle(context));
  second->idle();
  CHECK(rig.objects().queues_idle(context));
  elsewhere->busy();
  CHECK(rig.objects().queues_idle(context));
  CHECK(!rig.objects().queues_idle(other));
  CHECK(rig.objects().invariants_hold()); }

TEST(cl_object, an_orphan_with_no_children_does_not_owe_the_parent_one)
{ av::MockPlatform platform;
  ClObject parent(dispatch, platform, ClObject::Kind::command_queue, 0x900u, nullptr, nullptr);
  CHECK(parent.release());
  CHECK(parent.unheld());
  parent.orphan();
  CHECK(parent.unheld());
  parent.adopt();
  CHECK(!parent.unheld());
  parent.orphan();
  CHECK(parent.unheld()); }
