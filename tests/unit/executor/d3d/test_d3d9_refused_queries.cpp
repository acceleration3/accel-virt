#include "executor/d3d/d3d9_refused_queries.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::D3d9RefusedQueries;
using av::MockPlatform;

TEST(d3d9_refused_queries, remembers_the_failure_of_a_query_by_its_id_until_it_is_forgotten)
{ MockPlatform platform;
  D3d9RefusedQueries refused(platform);
  std::uint32_t hresult = 0u;
  CHECK(!refused.answer(7u, hresult));
  CHECK(refused.note(7u, 0x8876086au));
  CHECK(refused.answer(7u, hresult));
  CHECK_EQ(hresult, 0x8876086au);
  CHECK_EQ(refused.count(), 1u);
  CHECK(refused.forget(7u));
  CHECK(!refused.forget(7u));
  CHECK(!refused.answer(7u, hresult));
  CHECK(refused.invariants_hold()); }

TEST(d3d9_refused_queries, an_id_noted_again_takes_the_new_failure_and_id_zero_is_never_noted)
{ MockPlatform platform;
  D3d9RefusedQueries refused(platform);
  std::uint32_t hresult = 0u;
  CHECK(refused.note(7u, 1u));
  CHECK(refused.note(7u, 2u));
  CHECK(refused.answer(7u, hresult));
  CHECK_EQ(hresult, 2u);
  CHECK_EQ(refused.count(), 1u);
  CHECK(!refused.note(0u, 1u));
  CHECK_EQ(refused.count(), 1u); }

TEST(d3d9_refused_queries, holds_no_more_than_its_most_and_clearing_forgets_all)
{ MockPlatform platform;
  D3d9RefusedQueries refused(platform);
  for (std::uint32_t id = 1u; id <= D3d9RefusedQueries::most; id++) CHECK(refused.note(id, id));
  CHECK(!refused.note(D3d9RefusedQueries::most + 1u, 1u));
  CHECK_EQ(refused.count(), D3d9RefusedQueries::most);
  refused.clear();
  CHECK_EQ(refused.count(), 0u);
  CHECK(refused.note(5u, 1u));
  CHECK(refused.invariants_hold()); }

TEST(d3d9_refused_queries, a_note_there_is_no_memory_for_is_refused_and_leaves_the_old_one_gone)
{ MockPlatform platform;
  D3d9RefusedQueries refused(platform);
  platform.fail_allocation_at(1u);
  CHECK(!refused.note(7u, 1u));
  std::uint32_t hresult = 0u;
  CHECK(!refused.answer(7u, hresult));
  CHECK(refused.invariants_hold()); }
