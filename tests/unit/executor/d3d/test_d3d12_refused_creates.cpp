#include "executor/d3d/d3d12_refused_creates.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12RefusedCreates;

TEST(d3d12_refused_creates, a_noted_id_is_held_with_its_answer_until_forgotten)
{ D3d12RefusedCreates refused;
  CHECK(refused.note(7u, 0x8007000Eu));
  CHECK(refused.holds(7u));
  CHECK_EQ(refused.hresult_of(7u), 0x8007000Eu);
  CHECK(!refused.holds(8u));
  CHECK(refused.forget(7u));
  CHECK(!refused.holds(7u));
  CHECK(!refused.forget(7u));
  CHECK_EQ(refused.count(), 0u);
  CHECK(refused.invariants_hold()); }

TEST(d3d12_refused_creates, an_id_is_noted_once_and_the_table_refuses_past_its_capacity)
{ D3d12RefusedCreates refused;
  CHECK(refused.note(1u, 1u));
  CHECK(!refused.note(1u, 2u));
  CHECK_EQ(refused.hresult_of(1u), 1u);
  for (std::uint32_t id = 2u; id <= D3d12RefusedCreates::most; id++) CHECK(refused.note(id, id));
  CHECK_EQ(refused.count(), D3d12RefusedCreates::most);
  CHECK(!refused.note(1000u, 1u));
  CHECK(refused.forget(10u));
  CHECK(refused.note(1000u, 1u));
  CHECK(refused.holds(1u) && refused.holds(D3d12RefusedCreates::most) && refused.holds(1000u));
  CHECK(refused.invariants_hold());
  refused.clear();
  CHECK_EQ(refused.count(), 0u); }
