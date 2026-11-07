#include "executor/host_ram_ledger.hpp"
#include "executor/vram_gate.hpp"
#include "executor/vram_ledger.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/failing_blocks.hpp"

#include <cstdint>

using av::VramGate;

namespace {

constexpr std::uint64_t gib = 1ull << 30;

const std::uint8_t card[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
const std::uint8_t other[16] = {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};

struct Rig {
    Rig() : apis(), blocks(), gate(apis, blocks)
    { apis.add_gpu(card, 1u);
      apis.ledger().learn(16 * gib);
      apis.host_ram().budget(10 * gib); }

    av::MockHostApis  apis;
    av::FailingBlocks blocks;
    VramGate          gate;
};

}

TEST(vram_gate, the_table_tells_the_embedder_its_size_and_every_callback)
{ Rig rig;
  const AvVramGate &abi = rig.gate.abi();
  CHECK_EQ(abi.size, sizeof(AvVramGate));
  CHECK(abi.self == &rig.gate);
  CHECK(abi.open != nullptr && abi.take != nullptr && abi.give != nullptr && abi.close != nullptr);
  CHECK(abi.free_bytes != nullptr && abi.cap != nullptr);
  CHECK(abi.take_host != nullptr && abi.give_host != nullptr); }

TEST(vram_gate, an_account_opens_for_a_known_card_and_not_for_an_unknown_one)
{ Rig rig;
  CHECK(rig.gate.abi().open(rig.gate.abi().self, card) != nullptr);
  CHECK(rig.gate.abi().open(rig.gate.abi().self, other) == nullptr);
  CHECK(rig.gate.abi().open(rig.gate.abi().self, nullptr) == nullptr);
  CHECK_EQ(rig.gate.live(), 1u); }

TEST(vram_gate, takes_through_the_table_are_refused_past_the_cap_with_a_zero_answer)
{ Rig rig;
  const AvVramGate &abi = rig.gate.abi();
  void *const account = abi.open(abi.self, card);
  CHECK_EQ(abi.take(account, 10 * gib), 1);
  CHECK_EQ(abi.take(account, 5 * gib), 0);
  CHECK_EQ(abi.cap(account), 14 * gib);
  CHECK_EQ(abi.free_bytes(account), 4 * gib);
  abi.give(account, 10 * gib);
  CHECK_EQ(abi.take(account, 14 * gib), 1);
  CHECK_EQ(rig.apis.ledger().used(), 14 * gib); }

TEST(vram_gate, a_missing_account_means_no_cap_for_that_device)
{ Rig rig;
  const AvVramGate &abi = rig.gate.abi();
  CHECK_EQ(abi.take(nullptr, 1000 * gib), 1);
  abi.give(nullptr, gib);
  CHECK_EQ(abi.cap(nullptr), UINT64_MAX);
  CHECK_EQ(abi.free_bytes(nullptr), UINT64_MAX); }

TEST(vram_gate, closing_an_account_returns_what_its_device_leaked_to_the_card)
{ Rig rig;
  const AvVramGate &abi = rig.gate.abi();
  void *const account = abi.open(abi.self, card);
  void *const peer = abi.open(abi.self, card);
  CHECK_EQ(abi.take(account, 6 * gib), 1);
  CHECK_EQ(abi.take(peer, 6 * gib), 1);
  abi.close(abi.self, account);
  CHECK_EQ(rig.gate.live(), 1u);
  CHECK_EQ(rig.gate.leaked(), 6 * gib);
  CHECK_EQ(rig.apis.ledger().used(), 6 * gib);
  CHECK_EQ(abi.take(peer, 8 * gib), 1);
  CHECK(rig.gate.invariants_hold()); }

TEST(vram_gate, closing_an_account_twice_or_a_stranger_changes_nothing)
{ Rig rig;
  const AvVramGate &abi = rig.gate.abi();
  void *const account = abi.open(abi.self, card);
  CHECK_EQ(abi.take(account, gib), 1);
  abi.close(abi.self, account);
  abi.close(abi.self, account);
  int stranger = 0;
  abi.close(abi.self, &stranger);
  abi.close(abi.self, nullptr);
  CHECK_EQ(rig.gate.leaked(), gib);
  CHECK_EQ(rig.gate.live(), 0u); }

TEST(vram_gate, an_account_that_cannot_be_recorded_is_no_account_and_leaves_the_books_clean)
{ Rig rig;
  rig.blocks.fail_from(rig.blocks.requests() + 1u);
  CHECK(rig.gate.abi().open(rig.gate.abi().self, card) == nullptr);
  CHECK_EQ(rig.gate.live(), 0u);
  CHECK(rig.gate.invariants_hold()); }

TEST(vram_gate, host_takes_through_the_table_come_from_the_vms_ledger_and_are_refused_past_the_budget)
{ Rig rig;
  const AvVramGate &abi = rig.gate.abi();
  void *const account = abi.open(abi.self, card);
  CHECK_EQ(abi.take_host(account, 6 * gib), 1);
  CHECK_EQ(abi.take_host(account, 5 * gib), 0);
  CHECK_EQ(rig.apis.host_ram().used(), 6 * gib);
  CHECK_EQ(rig.apis.ledger().used(), 0u);
  abi.give_host(account, 6 * gib);
  CHECK_EQ(abi.take_host(account, 10 * gib), 1);
  CHECK_EQ(abi.take(account, 10 * gib), 1);
  CHECK_EQ(rig.apis.ledger().used(), 10 * gib); }

TEST(vram_gate, one_vm_budget_is_shared_by_the_accounts_of_every_device)
{ Rig rig;
  const AvVramGate &abi = rig.gate.abi();
  void *const dxvk = abi.open(abi.self, card);
  void *const vkd3d = abi.open(abi.self, card);
  CHECK_EQ(abi.take_host(dxvk, 6 * gib), 1);
  CHECK_EQ(abi.take_host(vkd3d, 5 * gib), 0);
  CHECK_EQ(abi.take_host(vkd3d, 4 * gib), 1);
  CHECK_EQ(abi.take_host(dxvk, 1u), 0u); }

TEST(vram_gate, a_missing_account_means_no_host_budget_for_that_device)
{ Rig rig;
  const AvVramGate &abi = rig.gate.abi();
  CHECK_EQ(abi.take_host(nullptr, 1000 * gib), 1);
  abi.give_host(nullptr, gib);
  CHECK_EQ(rig.apis.host_ram().used(), 0u); }

TEST(vram_gate, closing_an_account_returns_the_host_ram_its_device_leaked)
{ Rig rig;
  const AvVramGate &abi = rig.gate.abi();
  void *const account = abi.open(abi.self, card);
  void *const peer = abi.open(abi.self, card);
  CHECK_EQ(abi.take_host(account, 6 * gib), 1);
  CHECK_EQ(abi.take_host(peer, 2 * gib), 1);
  abi.close(abi.self, account);
  CHECK_EQ(rig.gate.leaked_host(), 6 * gib);
  CHECK_EQ(rig.gate.leaked(), 0u);
  CHECK_EQ(rig.apis.host_ram().used(), 2 * gib);
  CHECK(rig.gate.invariants_hold()); }

TEST(gate_account, holds_one_account_for_the_card_and_one_for_the_vms_host_ram)
{ av::VramLedger vram;
  av::HostRamLedger host;
  vram.learn(16 * gib);
  host.budget(4 * gib);
  av::GateAccount account(vram, &host);
  CHECK(account.vram().take(5 * gib));
  CHECK(account.host().take(3 * gib));
  CHECK_EQ(vram.used(), 5 * gib);
  CHECK_EQ(host.used(), 3 * gib);
  CHECK_EQ(account.vram().close(), 5 * gib);
  CHECK_EQ(account.host().close(), 3 * gib);
  CHECK_EQ(host.used(), 0u); }

TEST(gate_account, with_no_host_ledger_the_host_side_takes_everything)
{ av::VramLedger vram;
  av::GateAccount account(vram, nullptr);
  CHECK(!account.host().bound());
  CHECK(account.host().take(1000 * gib)); }
