#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/common/chain_slots.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/shared_object_answer.hpp"

#include <cstdint>

namespace av::icd {
struct ChainSlotsProbe {
    static void set_count(ChainSlots &slots, std::uint32_t count) { slots.count_ = count; }
    static void set_memory(ChainSlots &slots, std::uint32_t slot, std::uint64_t memory) { slots.memories_[slot] = memory; }
    static void set_identity(ChainSlots &slots, std::uint32_t slot, std::uint64_t id) { slots.ids_[slot] = id; }
    static void set_bytes(ChainSlots &slots, std::uint32_t slot, std::uint64_t bytes) { slots.bytes_[slot] = bytes; }
    static void set_opened(ChainSlots &slots, bool opened) { slots.opened_ = opened; }
    static void set_width(ChainSlots &slots, std::uint32_t width) { slots.width_ = width; }
    static void set_height(ChainSlots &slots, std::uint32_t height) { slots.height_ = height; }
    static void forget_chains(ChainSlots &slots) { slots.chains_ = nullptr; }
    static void forget_window(ChainSlots &slots) { slots.window_ = nullptr; }
};

}

using av::CommandHeader;
using av::MockTransport;
using av::ScriptedWindowChains;
using av::SharedObjectAnswer;
using av::icd::ChainSlots;
using av::icd::ChainSlotsProbe;
using av::icd::SlotLayout;

namespace {

constexpr std::uint64_t luid = 0x0000000200001234ull;
constexpr std::uint64_t opened_memory = 0x7700u;
constexpr std::uint32_t width = 64u;
constexpr std::uint32_t height = 32u;

void *window() { return reinterpret_cast<void *>(0x10u); }
void *other_window() { return reinterpret_cast<void *>(0x20u); }

void answer(MockTransport &wire, std::uint32_t status, std::uint64_t handle)
{ std::uint8_t reply[SharedObjectAnswer::encoded_size] = {};
  CHECK_STATUS(SharedObjectAnswer(status, handle).encode(reply, sizeof reply), av::Status::ok);
  wire.set_reply(reply, sizeof reply); }

class Rig {
public:
    Rig() : chains_(), wire_(), slots_()
    { chains_.refuse_opens(false);
      answer(wire_, SharedObjectAnswer::done, opened_memory); }

    ScriptedWindowChains &chains() { return chains_; }
    MockTransport        &wire() { return wire_; }
    ChainSlots           &slots() { return slots_; }
    bool open(std::uint32_t count) { return slots_.open(wire_, chains_, window(), luid, width, height, SlotLayout::bgra_top_down, count, 0x99u); }

private:
    ScriptedWindowChains chains_;
    MockTransport        wire_;
    ChainSlots           slots_;
};

}

TEST(chain_slots, a_request_the_window_chains_cannot_take_is_refused_before_it_is_asked)
{ Rig rig;
  ChainSlots &slots = rig.slots();
  CHECK(!slots.open(rig.wire(), rig.chains(), nullptr, luid, width, height, SlotLayout::bgra_top_down, 1u, 0u));
  CHECK(!slots.open(rig.wire(), rig.chains(), window(), luid, 0u, height, SlotLayout::bgra_top_down, 1u, 0u));
  CHECK(!slots.open(rig.wire(), rig.chains(), window(), luid, width, 0u, SlotLayout::bgra_top_down, 1u, 0u));
  CHECK(!slots.open(rig.wire(), rig.chains(), window(), luid, width, height, SlotLayout::bgra_top_down, 0u, 0u));
  CHECK(!slots.open(rig.wire(), rig.chains(), window(), luid, width, height, SlotLayout::bgra_top_down,
                    ChainSlots::max_slots + 1u, 0u));
  CHECK_EQ(rig.chains().opens(), 0u);
  CHECK_EQ(rig.wire().sends(), 0u);
  CHECK(!slots.opened());
  CHECK(slots.invariants_hold()); }

TEST(chain_slots, an_opened_set_answers_what_fits_and_what_each_slot_holds)
{ Rig rig;
  ChainSlots &slots = rig.slots();
  CHECK(!slots.fits(window(), width, height));
  CHECK(rig.open(2u));
  CHECK(slots.invariants_hold());
  CHECK(slots.opened());
  CHECK_EQ(slots.count(), 2u);
  CHECK_EQ(slots.width(), width);
  CHECK_EQ(slots.height(), height);
  CHECK(slots.fits(window(), width, height));
  CHECK(!slots.fits(other_window(), width, height));
  CHECK(!slots.fits(window(), width + 1u, height));
  CHECK(!slots.fits(window(), width, height + 1u));
  CHECK_EQ(slots.memory(0u), opened_memory);
  CHECK_EQ(slots.memory(1u), opened_memory);
  CHECK_EQ(slots.memory(ChainSlots::max_slots), 0u);
  CHECK_EQ(rig.wire().sends(), 2u);
  CHECK(slots.present(1u));
  CHECK(!slots.present(2u));
  CHECK_EQ(rig.chains().presents(), 1u); }

TEST(chain_slots, opening_the_same_window_on_another_chain_closes_the_old_chain_first)
{ Rig rig;
  ScriptedWindowChains second;
  second.refuse_opens(false);
  CHECK(rig.open(1u));
  CHECK(rig.slots().open(rig.wire(), second, window(), luid, width, height, SlotLayout::bgra_top_down, 1u, 0x99u));
  CHECK_EQ(rig.chains().closes(), 1u);
  CHECK(rig.chains().last_closed() == window());
  CHECK_EQ(second.opens(), 1u);
  CHECK_EQ(second.closes(), 0u);
  CHECK(rig.slots().invariants_hold());
  CHECK(rig.slots().open(rig.wire(), second, window(), luid, width, height, SlotLayout::bgra_top_down, 1u, 0x99u));
  CHECK_EQ(second.closes(), 0u);
  CHECK(rig.slots().open(rig.wire(), second, other_window(), luid, width, height, SlotLayout::bgra_top_down, 1u, 0x99u));
  CHECK_EQ(second.closes(), 1u);
  CHECK(second.last_closed() == window());
  CHECK(rig.slots().invariants_hold()); }

TEST(chain_slots, a_slot_the_host_will_not_open_closes_the_whole_set)
{ Rig rig;
  answer(rig.wire(), SharedObjectAnswer::unknown_object, 0u);
  CHECK(!rig.open(2u));
  CHECK(!rig.slots().opened());
  CHECK_EQ(rig.chains().closes(), 1u);
  CHECK_EQ(rig.slots().memory(0u), 0u);
  CHECK(rig.slots().invariants_hold()); }

TEST(chain_slots, its_invariants_catch_each_kind_of_corruption)
{ Rig rig;
  ChainSlots &slots = rig.slots();
  CHECK(rig.open(2u));
  CHECK(slots.invariants_hold());
  ChainSlotsProbe::set_count(slots, ChainSlots::max_slots + 1u);
  CHECK(!slots.invariants_hold());
  ChainSlotsProbe::set_count(slots, 1u);
  CHECK(!slots.invariants_hold());
  ChainSlotsProbe::set_count(slots, 2u);
  ChainSlotsProbe::set_memory(slots, 1u, 0u);
  CHECK(!slots.invariants_hold());
  ChainSlotsProbe::set_memory(slots, 1u, opened_memory);
  CHECK(slots.invariants_hold());
  ChainSlotsProbe::set_bytes(slots, 0u, 0u);
  CHECK(!slots.invariants_hold());
  ChainSlotsProbe::forget_chains(slots);
  CHECK(!slots.invariants_hold()); }

TEST(chain_slots, an_opened_set_needs_a_window_a_size_and_a_count)
{ Rig window_less;
  CHECK(window_less.open(1u));
  ChainSlotsProbe::forget_chains(window_less.slots());
  ChainSlotsProbe::forget_window(window_less.slots());
  CHECK(!window_less.slots().invariants_hold());

  Rig wide_less;
  CHECK(wide_less.open(1u));
  ChainSlotsProbe::set_width(wide_less.slots(), 0u);
  CHECK(!wide_less.slots().invariants_hold());

  Rig tall_less;
  CHECK(tall_less.open(1u));
  ChainSlotsProbe::set_height(tall_less.slots(), 0u);
  CHECK(!tall_less.slots().invariants_hold());

  Rig empty;
  CHECK(empty.open(1u));
  ChainSlotsProbe::set_count(empty.slots(), 0u);
  ChainSlotsProbe::set_memory(empty.slots(), 0u, 0u);
  CHECK(!empty.slots().invariants_hold());

  Rig shut;
  CHECK(shut.open(1u));
  ChainSlotsProbe::set_opened(shut.slots(), false);
  CHECK(shut.slots().invariants_hold()); }
