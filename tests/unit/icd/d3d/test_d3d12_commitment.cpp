#include "harness/framework/test_registry.hpp"
#include "icd/d3d/d3d12_commitment.hpp"

#include <cstdint>

using av::d3d::D3d12Commitment;
using Admission = D3d12Commitment::Admission;

namespace {
constexpr std::uint64_t mib = std::uint64_t{1} << 20u;
constexpr std::uint64_t gib = std::uint64_t{1} << 30u;
}

TEST(d3d12_commitment, a_create_well_inside_the_budget_is_deferred_and_counted)
{ D3d12Commitment commitment;
  commitment.learn(8u * gib);
  CHECK(commitment.charge(64u * mib) == Admission::defer);
  CHECK(commitment.charge(1u) == Admission::defer);
  CHECK_EQ(commitment.committed(), 64u * mib + 1u);
  commitment.release(64u * mib + 1u);
  CHECK_EQ(commitment.committed(), 0u);
  CHECK(commitment.invariants_hold()); }

TEST(d3d12_commitment, a_create_past_the_budget_is_refused_and_charges_nothing)
{ D3d12Commitment commitment;
  commitment.learn(gib);
  CHECK(commitment.charge(gib + 1u) == Admission::refused);
  CHECK(commitment.charge(600u * mib) == Admission::ask);
  CHECK(commitment.charge(500u * mib) == Admission::refused);
  CHECK_EQ(commitment.committed(), 600u * mib);
  CHECK(commitment.charge(424u * mib) == Admission::ask);
  CHECK_EQ(commitment.committed(), gib);
  CHECK(commitment.charge(1u) == Admission::refused);
  CHECK(commitment.invariants_hold()); }

TEST(d3d12_commitment, a_large_create_or_one_near_the_budget_is_asked_of_the_host)
{ D3d12Commitment commitment;
  commitment.learn(16u * gib);
  CHECK(commitment.charge(D3d12Commitment::ask_from_bytes - 1u) == Admission::defer);
  CHECK(commitment.charge(D3d12Commitment::ask_from_bytes) == Admission::ask);
  commitment.release(commitment.committed());
  commitment.learn(gib);
  for (unsigned step = 0u; step < 4u; step++) CHECK(commitment.charge(200u * mib) == Admission::defer);
  CHECK(commitment.charge(200u * mib) == Admission::ask);
  CHECK(commitment.charge(200u * mib) == Admission::refused); }

TEST(d3d12_commitment, an_unknown_budget_asks_every_time_and_a_release_never_underflows)
{ D3d12Commitment commitment;
  CHECK(commitment.charge(4096u) == Admission::ask);
  commitment.release(1u << 20u);
  CHECK_EQ(commitment.committed(), 0u);
  CHECK(commitment.invariants_hold()); }
