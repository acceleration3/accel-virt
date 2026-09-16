#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/scope_name.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "core/trace/trace_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>
#include <cstring>

using av::FunctionScope;
using av::MockTraceHost;
using av::ScopeBinding;
using av::ScopeName;
using av::Status;
using av::TraceSnapshot;
using av::TraceTable;

namespace {

constexpr std::size_t one_slot = 96u + 56u + 32u * 24u;

struct BoundTable {
    MockTraceHost host;
    alignas(8) std::uint8_t bytes[one_slot] = {};
    TraceTable    table{host};

    BoundTable()
    {
        CHECK_STATUS(table.initialize(bytes, sizeof bytes, "test", 1u), Status::ok);
        av::scope_binding.bind(table);
    }
    ~BoundTable() { av::scope_binding.unbind(); }
    TraceSnapshot snapshot() const
    {
        TraceSnapshot read;
        CHECK_STATUS(read.read(bytes, sizeof bytes), Status::ok);
        return read;
    }
};

std::uint32_t depth_inside_marked_function(const BoundTable &bound)
{
    AV_SCOPE("depth_inside_marked_function");
    return bound.snapshot().slot(0u).depth();
}

}  // namespace

/* FNV-1a 32's published vectors: the ids a reader recomputes from source. */
TEST(scope_name, its_id_is_fnv1a_of_the_name_and_computed_while_compiling)
{ constexpr ScopeName empty("");
  constexpr ScopeName a("a");
  constexpr ScopeName foobar("foobar");
  constexpr ScopeName none(nullptr);
  static_assert(foobar.id() == 0xBF9CF968u);
  CHECK_EQ(empty.id(), 0x811C9DC5u);
  CHECK_EQ(a.id(), 0xE40C292Cu);
  CHECK_EQ(foobar.id(), 0xBF9CF968u);
  CHECK_EQ(none.id(), 0x811C9DC5u);
  CHECK(std::strcmp(foobar.text(), "foobar") == 0);
  CHECK(none.text() == nullptr);
  /* The same names made while running, as a name from a variable would be. */
  const char *volatile_names[] = {nullptr, "foobar"};
  const ScopeName runtime_none(volatile_names[0]);
  const ScopeName runtime_foobar(volatile_names[1]);
  CHECK_EQ(runtime_none.id(), 0x811C9DC5u);
  CHECK_EQ(runtime_foobar.id(), 0xBF9CF968u); }

TEST(scope_binding, holds_the_table_until_unbound)
{ ScopeBinding binding;
  CHECK(binding.table() == nullptr);
  MockTraceHost host;
  TraceTable table(host);
  binding.bind(table);
  CHECK(binding.table() == &table);
  binding.unbind();
  CHECK(binding.table() == nullptr);
  CHECK(av::scope_binding.table() == nullptr); }

TEST(function_scope, an_unbound_scope_does_nothing)
{ MockTraceHost host;
  ScopeBinding unbound;
  { const FunctionScope scope(unbound, ScopeName("Nothing::here")); }
  CHECK_EQ(host.exchanges(), 0u); }

TEST(function_scope, the_macro_pushes_the_named_frame_for_the_function_s_life)
{ BoundTable bound;
  CHECK_EQ(depth_inside_marked_function(bound), 1u);
  const TraceSnapshot after = bound.snapshot();
  CHECK(!after.slot(0u).active());
  CHECK_EQ(after.slot(0u).depth(), 0u);
  CHECK_EQ(after.slot(0u).last_left(), ScopeName("depth_inside_marked_function").id());
  CHECK(bound.table.invariants_hold()); }

TEST(function_scope, nested_scopes_pop_in_order)
{ BoundTable bound;
  {
    const FunctionScope outer(av::scope_binding, ScopeName("Outer::run"));
    {
      const FunctionScope inner(av::scope_binding, ScopeName("Inner::leaf"));
      CHECK_EQ(bound.snapshot().slot(0u).depth(), 2u);
      CHECK_EQ(bound.snapshot().slot(0u).name(1u), ScopeName("Inner::leaf").id());
    }
    CHECK_EQ(bound.snapshot().slot(0u).depth(), 1u);
    CHECK(bound.table.invariants_hold());
  }
  CHECK_EQ(bound.snapshot().slot(0u).depth(), 0u);
  CHECK_EQ(bound.snapshot().slot(0u).last_left(), ScopeName("Outer::run").id()); }

/* A scope that got no slot leaves nothing on the way out, even once a slot is
 * free again: its leave would pop another function's frame. */
TEST(function_scope, a_scope_without_a_slot_pops_nothing)
{ BoundTable bound;
  bound.host.run_as(0x10u, 1u);
  const FunctionScope holder(av::scope_binding, ScopeName("Holder::run"));
  bound.host.run_as(0x20u, 2u);
  {
    const FunctionScope refused(av::scope_binding, ScopeName("Refused::run"));
    bound.host.run_as(0x10u, 1u);
    CHECK_EQ(bound.snapshot().slot(0u).depth(), 1u);
  }
  CHECK_EQ(bound.snapshot().slot(0u).depth(), 1u);
  CHECK(bound.snapshot().slot(0u).active()); }
