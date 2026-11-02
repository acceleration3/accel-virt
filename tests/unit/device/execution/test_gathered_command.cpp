#include "core/support/status.hpp"
#include "device/execution/gathered_command.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::GatheredCommand;
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
