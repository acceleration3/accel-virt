#include "guest/inspect/inspect_host.hpp"
#include "guest/inspect/snapshot_file.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_inspect_host.hpp"

#include <cstdint>
#include <cstring>

using av::MockInspectHost;
using av::SnapshotFile;
using av::Status;

TEST(snapshot_file, writes_the_documented_header_then_the_payload)
{ MockInspectHost host;
  av::InspectHost &seam = host;
  SnapshotFile file(seam, "C:\\rvk\\out.bin");
  const std::uint8_t payload[] = {1, 2, 3};
  CHECK_STATUS(file.write(SnapshotFile::Kind::table, 0x1122334455667788ull, payload, sizeof payload), Status::ok);
  const std::uint8_t expected[] = {'R', 'V', 'K', 'S', 'N', 'A', 'P', '1', 0x88, 0x77, 0x66, 0x55, 0x44, 0x33,
                                   0x22, 0x11, 1, 0, 0, 0, 3, 0, 0, 0, 1, 2, 3};
  CHECK_EQ(host.written_size(), sizeof expected);
  CHECK(std::memcmp(host.written(), expected, sizeof expected) == 0);
  CHECK(std::strcmp(host.written_path(), "C:\\rvk\\out.bin") == 0); }

TEST(snapshot_file, an_error_file_carries_its_text)
{ MockInspectHost host;
  SnapshotFile file(host, "e");
  CHECK_STATUS(file.refuse(9u, "no table"), Status::ok);
  CHECK_EQ(host.written_size(), 24u + 8u);
  CHECK_EQ(host.written()[16], 3u);
  CHECK_EQ(host.written()[20], 8u);
  CHECK(std::memcmp(host.written() + 24, "no table", 8u) == 0);
  CHECK_STATUS(file.refuse(9u, ""), Status::ok);
  CHECK_EQ(host.written_size(), 24u); }

TEST(snapshot_file, refuses_a_payload_a_u32_cannot_count_and_passes_on_a_write_failure)
{ MockInspectHost host;
  SnapshotFile file(host, "e");
  const std::uint8_t byte = 0u;
  CHECK_STATUS(file.write(SnapshotFile::Kind::state, 0u, &byte, 0x100000000ull), Status::too_large);
  CHECK_EQ(host.writes(), 0u);
  CHECK_STATUS(file.write(SnapshotFile::Kind::state, 0u, &byte, 0xFFFFFFFFull - 0xFFFFFFFFull + 1u), Status::ok);
  host.fail_next_write(Status::no_space);
  CHECK_STATUS(file.write(SnapshotFile::Kind::state, 0u, &byte, 1u), Status::no_space);
  CHECK_EQ(host.writes(), 2u); }
