/* The context states of one session's devices: kept by the guest's id, found
 * by it, and given back one at a time or all at once when the session closes.
 * An id of 0, one kept twice or a full table refuses; invariants after every
 * mutation. The states are never touched, so stand-ins do. */
#include "device/executor/d3d_context_states.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3dContextStates;

namespace {

struct ID3DDeviceContextState *stand_in(std::uint32_t index)
{
    static std::uint8_t states[D3dContextStates::capacity + 1u];
    return reinterpret_cast<ID3DDeviceContextState *>(&states[index]);
}

}  // namespace

TEST(d3d_context_states, keeps_finds_and_gives_back_each_device_s_state)
{ D3dContextStates states;
  CHECK(states.add(7u, stand_in(0u)));
  CHECK(states.add(9u, stand_in(1u)));
  CHECK(states.invariants_hold());
  CHECK_EQ(states.count(), 2u);
  CHECK(states.find(7u) == stand_in(0u));
  CHECK(states.find(9u) == stand_in(1u));
  CHECK(states.find(8u) == nullptr);
  CHECK(states.remove(7u) == stand_in(0u));
  CHECK(states.invariants_hold());
  CHECK_EQ(states.count(), 1u);
  CHECK(states.find(7u) == nullptr);
  CHECK(states.remove(7u) == nullptr);
  CHECK(states.find(9u) == stand_in(1u)); }

TEST(d3d_context_states, refuses_id_zero_a_null_state_and_an_id_kept_twice)
{ D3dContextStates states;
  CHECK(!states.add(0u, stand_in(0u)));
  CHECK(!states.add(3u, nullptr));
  CHECK(states.add(3u, stand_in(0u)));
  CHECK(!states.add(3u, stand_in(1u)));
  CHECK(states.find(3u) == stand_in(0u));
  CHECK(states.find(0u) == nullptr);
  CHECK(states.remove(0u) == nullptr);
  CHECK_EQ(states.count(), 1u);
  CHECK(states.invariants_hold()); }

TEST(d3d_context_states, a_full_table_refuses_the_next_and_takes_one_after_a_removal)
{ D3dContextStates states;
  for (std::uint32_t index = 0u; index < D3dContextStates::capacity; index++)
      CHECK(states.add(index + 1u, stand_in(index)));
  CHECK(!states.add(D3dContextStates::capacity + 1u, stand_in(D3dContextStates::capacity)));
  CHECK(states.remove(5u) == stand_in(4u));
  CHECK(states.add(D3dContextStates::capacity + 1u, stand_in(D3dContextStates::capacity)));
  CHECK(states.find(D3dContextStates::capacity + 1u) == stand_in(D3dContextStates::capacity));
  CHECK(states.invariants_hold()); }

TEST(d3d_context_states, remove_any_gives_every_state_back_once)
{ D3dContextStates states;
  CHECK(states.add(4u, stand_in(0u)));
  CHECK(states.add(2u, stand_in(1u)));
  std::uint32_t given = 0u;
  bool seen[2] = {};
  for (ID3DDeviceContextState *state = states.remove_any(); state != nullptr; state = states.remove_any()) {
      if (state == stand_in(0u)) seen[0] = true;
      if (state == stand_in(1u)) seen[1] = true;
      given++;
      CHECK(states.invariants_hold());
  }
  CHECK_EQ(given, 2u);
  CHECK(seen[0] && seen[1]);
  CHECK_EQ(states.count(), 0u); }

namespace av {

struct D3dContextStatesProbe {
    static void set_id(D3dContextStates &s, std::uint32_t slot, std::uint32_t id) { s.ids_[slot] = id; }
    static void set_count(D3dContextStates &s, std::uint32_t count) { s.count_ = count; }
};

}  // namespace av

TEST(d3d_context_states_invariants, detect_an_id_without_a_state_a_duplicate_and_a_wrong_count)
{ for (int knob = 0; knob < 3; knob++) {
      D3dContextStates states;
      CHECK(states.add(1u, stand_in(0u)));
      CHECK(states.add(2u, stand_in(1u)));
      if (knob == 0) av::D3dContextStatesProbe::set_id(states, 5u, 3u);   /* an id with no state */
      if (knob == 1) av::D3dContextStatesProbe::set_id(states, 1u, 1u);   /* 1 kept twice */
      if (knob == 2) av::D3dContextStatesProbe::set_count(states, 3u);
      CHECK(!states.invariants_hold());
  } }
