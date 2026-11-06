#include "core/support/status.hpp"
#include "device/execution/gathered_command.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace av {

struct GatheredCommandProbe {
    static void overuse(GatheredCommand &gathered) { gathered.used_ = gathered.capacity_ + 1u; }
    static void lose_block(GatheredCommand &gathered) { gathered.capacity_ = 0u; gathered.used_ = 0u; }
    static void forget_block(GatheredCommand &gathered) { gathered.block_ = nullptr; }
    static void restore(GatheredCommand &gathered, std::uint8_t *block, std::size_t capacity, std::size_t used)
    {
        gathered.block_ = block;
        gathered.capacity_ = capacity;
        gathered.used_ = used;
    }
    static std::uint8_t *block(GatheredCommand &gathered) { return gathered.block_; }
    static std::size_t capacity(GatheredCommand &gathered) { return gathered.capacity_; }
};

}

using av::GatheredCommand;
using av::GatheredCommandProbe;
using av::MockHost;
using av::Status;

TEST(gathered_command, pieces_and_the_closing_command_make_one_payload)
{ MockHost        host;
  GatheredCommand gathered(host);
  CHECK(!gathered.gathering());
  static std::uint8_t piece[600u * 1024u];
  for (std::size_t index = 0u; index < sizeof piece; index++) piece[index] = static_cast<std::uint8_t>(index * 3u);
  CHECK_STATUS(gathered.gather(piece, sizeof piece), Status::ok);
  CHECK(gathered.gathering());
  CHECK_STATUS(gathered.gather(piece, sizeof piece), Status::ok);
  CHECK_STATUS(gathered.gather(piece, 0u), Status::ok);
  const std::uint8_t last[3] = {7u, 8u, 9u};
  const std::uint8_t *payload = nullptr;
  std::size_t         size = 0u;
  CHECK_STATUS(gathered.close(last, sizeof last, payload, size), Status::ok);
  CHECK_EQ(size, 2u * sizeof piece + 3u);
  CHECK(payload != nullptr);
  CHECK(std::memcmp(payload, piece, sizeof piece) == 0);
  CHECK(std::memcmp(payload + sizeof piece, piece, sizeof piece) == 0);
  CHECK_EQ(payload[size - 1u], 9u);
  CHECK(gathered.invariants_hold());
  gathered.clear();
  CHECK(!gathered.gathering());
  CHECK(gathered.invariants_hold()); }

TEST(gathered_command, no_memory_drops_what_was_gathered)
{ MockHost        host;
  GatheredCommand gathered(host);
  const std::uint8_t piece[16] = {};
  host.fail_allocation_at(1u);
  CHECK_STATUS(gathered.gather(piece, sizeof piece), Status::no_memory);
  CHECK(!gathered.gathering());
  CHECK_STATUS(gathered.gather(piece, sizeof piece), Status::ok);
  host.fail_allocation_at(1u);
  static std::uint8_t big[2u * 1024u * 1024u];
  const std::uint8_t *payload = reinterpret_cast<const std::uint8_t *>(&gathered);
  std::size_t         size = 1u;
  CHECK_STATUS(gathered.close(big, sizeof big, payload, size), Status::no_memory);
  CHECK(payload == nullptr);
  CHECK_EQ(size, 0u);
  CHECK(!gathered.gathering());
  CHECK(gathered.invariants_hold()); }

TEST(gathered_command, a_block_already_past_the_least_one_doubles_when_more_is_gathered)
{ MockHost        host;
  GatheredCommand gathered(host);
  static std::uint8_t piece[800u * 1024u];
  for (std::size_t index = 0u; index < sizeof piece; index++) piece[index] = static_cast<std::uint8_t>(index);
  CHECK_STATUS(gathered.gather(piece, sizeof piece), Status::ok);
  CHECK_STATUS(gathered.gather(piece, sizeof piece), Status::ok);
  CHECK_STATUS(gathered.gather(piece, sizeof piece), Status::ok);
  CHECK_EQ(GatheredCommandProbe::capacity(gathered), 4u * 1024u * 1024u);
  const std::uint8_t *payload = nullptr;
  std::size_t         size = 0u;
  CHECK_STATUS(gathered.close(piece, 1u, payload, size), Status::ok);
  CHECK_EQ(size, 3u * sizeof piece + 1u);
  CHECK(std::memcmp(payload + 2u * sizeof piece, piece, sizeof piece) == 0);
  CHECK(gathered.invariants_hold()); }

TEST(gathered_command, a_length_no_size_can_hold_after_what_was_gathered_drops_it_all)
{ MockHost        host;
  GatheredCommand gathered(host);
  const std::uint8_t piece[8] = {};
  CHECK_STATUS(gathered.gather(piece, sizeof piece), Status::ok);
  CHECK(gathered.gathering());
  CHECK_STATUS(gathered.gather(piece, ~std::size_t{0}), Status::no_memory);
  CHECK(!gathered.gathering());
  CHECK(gathered.invariants_hold()); }

TEST(gathered_command, a_length_past_half_the_address_space_is_asked_for_whole_and_may_be_refused)
{ MockHost        host;
  GatheredCommand gathered(host);
  const std::uint8_t piece[8] = {};
  host.fail_allocation_at(1u);
  CHECK_STATUS(gathered.gather(piece, ~std::size_t{0}), Status::no_memory);
  CHECK(!gathered.gathering());
  CHECK(gathered.invariants_hold()); }

TEST(gathered_command, the_invariant_catches_use_past_the_block_and_a_block_without_a_capacity)
{ MockHost        host;
  GatheredCommand gathered(host);
  const std::uint8_t piece[8] = {};
  CHECK_STATUS(gathered.gather(piece, sizeof piece), Status::ok);
  CHECK(gathered.invariants_hold());
  std::uint8_t *const block = GatheredCommandProbe::block(gathered);
  const std::size_t   capacity = GatheredCommandProbe::capacity(gathered);
  GatheredCommandProbe::overuse(gathered);
  CHECK(!gathered.invariants_hold());
  GatheredCommandProbe::restore(gathered, block, capacity, sizeof piece);
  GatheredCommandProbe::lose_block(gathered);
  CHECK(!gathered.invariants_hold());
  GatheredCommandProbe::restore(gathered, block, capacity, sizeof piece);
  GatheredCommandProbe::forget_block(gathered);
  CHECK(!gathered.invariants_hold());
  GatheredCommandProbe::restore(gathered, block, capacity, sizeof piece);
  CHECK(gathered.invariants_hold()); }
