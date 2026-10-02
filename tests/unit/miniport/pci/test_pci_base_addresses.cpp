#include "miniport/pci/pci_base_addresses.hpp"
#include "core/support/status.hpp"
#include "harness/framework/fixture_file.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::FixtureFile;
using av::PciBaseAddresses;
using av::Status;

namespace {

class ConfigHeader {
public:
    ConfigHeader() : bytes_() {}
    void put32(std::size_t offset, std::uint32_t value) { std::memcpy(bytes_ + offset, &value, 4u); }
    void bar(std::uint8_t index, std::uint32_t value) { put32(0x10u + 4u * index, value); }
    std::uint8_t *bytes() { return bytes_; }
private:
    std::uint8_t bytes_[64];
};

/* Every BAR is decoded, so none of them may read outside the snapshot, but the
 * verdict is BAR 4's alone: this device leaves the other five unassigned, and
 * refusing an unassigned BAR says something about the fixture rather than about
 * the bytes fed. */
bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    const PciBaseAddresses bars(bytes, length);
    std::uint64_t address = 0u;
    for (std::uint8_t bar = 0u; bar < PciBaseAddresses::bar_count; bar++) {
        const Status status = bars.address(bar, address);
        if (status != Status::ok && status != Status::not_found &&
            status != Status::not_supported && status != Status::malformed)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                        "a BAR answered a status the contract does not name");
    }
    std::uint64_t assigned = 0u;
    return bars.address(4u, assigned) != Status::ok;
}

}  // namespace

TEST(pci_base_addresses, reads_qemu_virtio_gpu_pci_64_bit_bar_4)
{ FixtureFile fixture("virtio_gpu_pci_config.bin");
  std::uint32_t low = 0x0000000Cu, high = 0x80u;
  std::memcpy(fixture.mutable_bytes() + 0x20u, &low, 4u);
  std::memcpy(fixture.mutable_bytes() + 0x24u, &high, 4u);
  const PciBaseAddresses bars(fixture.bytes(), fixture.size());
  std::uint64_t address = 0u;
  CHECK_STATUS(bars.address(4u, address), Status::ok);
  CHECK(address == 0x8000000000ull); }

TEST(pci_base_addresses, a_32_bit_memory_bar_drops_its_flag_bits)
{ ConfigHeader header;
  header.bar(1u, 0xFEA00008u);   /* 32-bit, prefetchable */
  const PciBaseAddresses bars(header.bytes(), 64u);
  std::uint64_t address = 0u;
  CHECK_STATUS(bars.address(1u, address), Status::ok);
  CHECK(address == 0xFEA00000u); }

TEST(pci_base_addresses, an_unassigned_bar_is_not_found)
{ ConfigHeader header;
  header.bar(2u, 0x0000000Cu);
  const PciBaseAddresses bars(header.bytes(), 64u);
  std::uint64_t address = 7u;
  CHECK_STATUS(bars.address(0u, address), Status::not_found);
  CHECK_STATUS(bars.address(2u, address), Status::not_found);
  CHECK(address == 7u); }

TEST(pci_base_addresses, an_io_bar_is_not_supported)
{ ConfigHeader header;
  header.bar(0u, 0x0000C001u);
  const PciBaseAddresses bars(header.bytes(), 64u);
  std::uint64_t address = 0u;
  CHECK_STATUS(bars.address(0u, address), Status::not_supported); }

TEST(pci_base_addresses, a_64_bit_bar_5_and_reserved_types_are_malformed)
{ ConfigHeader header;
  header.bar(5u, 0xF0000004u);   /* 64-bit with no upper half */
  header.bar(3u, 0xF0000002u);   /* below-1MB type, reserved since PCI 3.0 */
  header.bar(4u, 0xF0000006u);   /* type 3, reserved */
  const PciBaseAddresses bars(header.bytes(), 64u);
  std::uint64_t address = 0u;
  CHECK_STATUS(bars.address(5u, address), Status::malformed);
  CHECK_STATUS(bars.address(3u, address), Status::malformed);
  CHECK_STATUS(bars.address(4u, address), Status::malformed); }

TEST(pci_base_addresses, a_bar_index_past_five_is_an_invalid_parameter)
{ ConfigHeader header;
  const PciBaseAddresses bars(header.bytes(), 64u);
  std::uint64_t address = 0u;
  CHECK_STATUS(bars.address(6u, address), Status::invalid_parameter); }

TEST(pci_base_addresses, a_bridge_header_or_a_short_snapshot_is_malformed)
{ ConfigHeader header;
  header.bar(0u, 0xFEA00000u);
  header.bytes()[0x0Eu] = 0x81u;   /* multi-function, type 1 (bridge) */
  std::uint64_t address = 0u;
  CHECK_STATUS(PciBaseAddresses(header.bytes(), 64u).address(0u, address), Status::malformed);
  header.bytes()[0x0Eu] = 0x80u;   /* multi-function, type 0: fine */
  CHECK_STATUS(PciBaseAddresses(header.bytes(), 64u).address(0u, address), Status::ok);
  CHECK_STATUS(PciBaseAddresses(header.bytes(), 0x27u).address(0u, address), Status::malformed);
  CHECK_STATUS(PciBaseAddresses(nullptr, 64u).address(0u, address), Status::malformed); }

TEST(pci_base_addresses_hostile, survives_every_mutation_of_a_real_header)
{ FixtureFile fixture("virtio_gpu_pci_config.bin");
  std::uint32_t high = 0x80u;
  std::memcpy(fixture.mutable_bytes() + 0x24u, &high, 4u);
  const av::HostileSweep sweep(rejects, fixture.bytes(), 64u);
  sweep.run();
  CHECK(sweep.cases_run() > 500u);
  /* BAR 4 is this device's 64-bit memory window, and it survives a flip only
   * where the flip cannot reach it: the 14 bytes in front of the header type
   * (112 bits), the byte behind it (8), the four unassigned BARs (128), the 24
   * bytes of the header past the BARs (192), and the bit of the header type
   * byte that the type mask throws away (1). Of the BAR's own low register, the
   * three bits that make it I/O space, a reserved type or an unassigned 32-bit
   * BAR are refused and the other five accepted, its remaining three bytes only
   * moving the address (24); of its high register every flip but the one that
   * clears the whole address (31). A snapshot shorter than the 40 bytes the
   * BARs need is refused, the 24 longer truncations still hold them, all-zero
   * leaves BAR 4 unassigned, all-ones is not a type 0 header, and the untouched
   * header is the last acceptance. */
  const std::uint32_t acceptable =
      112u + 8u + 128u + 192u + 1u + 5u + 24u + 31u + 24u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
