#include "core/support/status.hpp"
#include "core/virtio/virtio_capability_walk.hpp"
#include "core/virtio/virtio_pci_layout.hpp"
#include "core/virtio/virtio_region.hpp"
#include "harness/framework/fixture_file.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::FixtureFile;
using av::Status;
using av::VirtioCapabilityWalk;
using av::VirtioPciLayout;
using av::VirtioRegion;

namespace {

/* A config space built from scratch: capability list at 0x40, entries appended. */
class ConfigSpace {
public:
    ConfigSpace()
    {
        bytes_[0x06] = 0x10u;   /* status: capability list present */
        bytes_[0x34] = 0x40u;
    }

    /* Appends a virtio capability; returns its offset. */
    std::size_t add(std::uint8_t type, std::uint8_t bar, std::uint32_t offset, std::uint32_t length,
                    std::uint8_t cap_len = 16u, std::uint8_t id = 0u)
    {
        const std::size_t at = next_;
        if (previous_ != 0u) bytes_[previous_ + 1u] = static_cast<std::uint8_t>(at);
        bytes_[at] = 0x09u; bytes_[at + 2u] = cap_len; bytes_[at + 3u] = type;
        bytes_[at + 4u] = bar; bytes_[at + 5u] = id;
        put32(at + 8u, offset); put32(at + 12u, length);
        previous_ = at;
        next_ = at + ((cap_len + 3u) & ~3u);
        return at;
    }

    void add_standard_four()
    {
        add(1u, 4u, 0x0000u, 0x1000u);
        add(3u, 4u, 0x1000u, 0x1000u);
        add(4u, 4u, 0x2000u, 0x1000u);
        put32(add(2u, 4u, 0x3000u, 0x1000u, 20u) + 16u, 4u);
    }

    void put32(std::size_t at, std::uint32_t value)
    {
        for (unsigned i = 0u; i < 4u; i++) bytes_[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
    }

    std::uint8_t *bytes() { return bytes_; }

    Status walk(VirtioPciLayout &layout) const
    {
        return VirtioCapabilityWalk(bytes_, sizeof bytes_).walk(layout);
    }

private:
    std::uint8_t bytes_[256] = {};
    std::size_t  next_ = 0x40u;
    std::size_t  previous_ = 0u;
};

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    VirtioPciLayout layout;
    return VirtioCapabilityWalk(bytes, length).walk(layout) != Status::ok;
}

void check_region(const VirtioRegion &region, std::uint8_t bar, std::uint64_t offset, std::uint64_t length)
{
    CHECK(region.present());
    CHECK_EQ(region.bar(), bar);
    CHECK(region.offset() == offset);
    CHECK(region.length() == length);
}

}  // namespace

TEST(virtio_capability_walk, parses_qemu_virtio_gpu_pci)
{ const FixtureFile fixture("virtio_gpu_pci_config.bin");
  CHECK_EQ(fixture.size(), 256u);
  VirtioPciLayout layout;
  CHECK_STATUS(VirtioCapabilityWalk(fixture.bytes(), fixture.size()).walk(layout), Status::ok);
  check_region(layout.common(), 4u, 0x0u, 0x1000u);
  check_region(layout.isr(), 4u, 0x1000u, 0x1000u);
  check_region(layout.device(), 4u, 0x2000u, 0x1000u);
  check_region(layout.notify(), 4u, 0x3000u, 0x1000u);
  CHECK_EQ(layout.notify_multiplier(), 4u);
  CHECK(!layout.shared_memory(1u).present()); }

TEST(virtio_capability_walk, parses_qemu_virtio_gpu_gl_pci_with_hostmem)
{ const FixtureFile fixture("virtio_gpu_gl_pci_hostmem_config.bin");
  VirtioPciLayout layout;
  CHECK_STATUS(VirtioCapabilityWalk(fixture.bytes(), fixture.size()).walk(layout), Status::ok);
  check_region(layout.common(), 2u, 0x0u, 0x1000u);
  check_region(layout.notify(), 2u, 0x3000u, 0x1000u);
  check_region(layout.shared_memory(1u), 4u, 0x0u, 0x10000000u);
  CHECK(!layout.shared_memory(2u).present()); }

TEST(virtio_capability_walk, the_first_instance_of_a_type_wins)
{ ConfigSpace space;
  space.add_standard_four();
  space.add(1u, 2u, 0x8000u, 0x1000u);
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::ok);
  check_region(layout.common(), 4u, 0x0u, 0x1000u); }

TEST(virtio_capability_walk, reserved_types_and_bars_are_ignored)
{ ConfigSpace space;
  space.add(1u, 6u, 0x0u, 0x1000u);          /* reserved bar: ignored */
  space.add(7u, 4u, 0x0u, 0x1000u);          /* reserved type */
  space.add(5u, 0u, 0x0u, 0x0u, 20u);        /* PCI cfg access: not used */
  space.add(9u, 4u, 0x0u, 0x10u);            /* vendor data */
  space.add_standard_four();
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::ok);
  CHECK_EQ(layout.common().bar(), 4u); }

TEST(virtio_capability_walk, non_virtio_capabilities_are_skipped)
{ ConfigSpace space;
  space.add_standard_four();
  space.bytes()[0x40] = 0x11u;               /* first becomes MSI-X */
  space.add(1u, 3u, 0x0u, 0x1000u);          /* so common comes from here */
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::ok);
  CHECK_EQ(layout.common().bar(), 3u); }

TEST(virtio_capability_walk, a_64_bit_shared_memory_region)
{ ConfigSpace space;
  space.add_standard_four();
  const std::size_t shm = space.add(8u, 2u, 0x1000u, 0x2000u, 24u, 7u);
  space.put32(shm + 16u, 0x1u);
  space.put32(shm + 20u, 0x2u);
  space.add(8u, 3u, 0x0u, 0x10u, 24u, 7u);    /* duplicate id: first wins */
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::ok);
  check_region(layout.shared_memory(7u), 2u, 0x100001000ull, 0x200002000ull); }

TEST(virtio_capability_walk, more_shared_memory_regions_than_kept_are_ignored)
{ ConfigSpace space;
  space.add_standard_four();
  for (std::uint8_t id = 0u; id <= VirtioPciLayout::max_shared_memory_regions; id++)
      space.add(8u, 2u, 0x0u, 0x1000u, 24u, id);
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::ok);
  CHECK(layout.shared_memory(0u).present());
  CHECK(!layout.shared_memory(VirtioPciLayout::max_shared_memory_regions).present()); }

TEST(virtio_capability_walk, a_zero_notify_multiplier_is_legal)
{ ConfigSpace space;
  space.add(1u, 4u, 0x0u, 0x1000u);
  space.add(3u, 4u, 0x1000u, 0x1000u);
  space.add(4u, 4u, 0x2000u, 0x1000u);
  space.add(2u, 4u, 0x3000u, 0x1000u, 20u);
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::ok);
  CHECK_EQ(layout.notify_multiplier(), 0u); }

TEST(virtio_capability_walk, a_notify_multiplier_that_is_not_an_even_power_of_two_is_malformed)
{ const std::uint32_t bad[] = {1u, 3u, 6u, 0x80000001u};
  for (const std::uint32_t multiplier : bad) {
      ConfigSpace space;
      space.add(1u, 4u, 0x0u, 0x1000u);
      space.add(3u, 4u, 0x1000u, 0x1000u);
      space.add(4u, 4u, 0x2000u, 0x1000u);
      space.put32(space.add(2u, 4u, 0x3000u, 0x1000u, 20u) + 16u, multiplier);
      VirtioPciLayout layout;
      CHECK_STATUS(space.walk(layout), Status::malformed);
  } }

TEST(virtio_capability_walk, missing_required_structures_are_not_supported)
{ for (std::uint8_t skip = 1u; skip <= 4u; skip++) {
      ConfigSpace space;
      for (std::uint8_t type = 1u; type <= 4u; type++) {
          if (type == skip) continue;
          space.add(type, 4u, 0x0u, 0x1000u, type == 2u ? 20u : 16u);
      }
      VirtioPciLayout layout;
      CHECK_STATUS(space.walk(layout), Status::not_supported);
  } }

TEST(virtio_capability_walk, no_capability_list_is_not_supported)
{ ConfigSpace space;
  space.add_standard_four();
  space.bytes()[0x06] = 0u;
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::not_supported);
  space.bytes()[0x06] = 0x10u;
  space.bytes()[0x34] = 0u;
  CHECK_STATUS(space.walk(layout), Status::not_supported); }

TEST(virtio_capability_walk, a_looping_list_is_malformed)
{ ConfigSpace space;
  space.add_standard_four();
  space.bytes()[0x41] = 0x40u;               /* first points at itself */
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::malformed);
  ConfigSpace longer;
  longer.add_standard_four();
  longer.bytes()[0x40u + 16u + 16u + 16u + 1u] = 0x50u;   /* last points back to the second */
  CHECK_STATUS(longer.walk(layout), Status::malformed); }

TEST(virtio_capability_walk, a_pointer_into_the_standard_header_is_malformed)
{ ConfigSpace space;
  space.add_standard_four();
  space.bytes()[0x34] = 0x3Cu;
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::malformed); }

TEST(virtio_capability_walk, a_capability_running_past_the_config_space_is_malformed)
{ ConfigSpace space;
  space.bytes()[0x34] = 0xF8u;
  space.bytes()[0xF8] = 0x09u;
  space.bytes()[0xFA] = 16u;
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::malformed); }

TEST(virtio_capability_walk, a_cap_len_shorter_than_its_structure_is_malformed)
{ struct Case { std::uint8_t type; std::uint8_t cap_len; };
  const Case cases[] = {{1u, 15u}, {2u, 19u}, {8u, 23u}};
  for (const Case &c : cases) {
      ConfigSpace space;
      space.add(c.type, 4u, 0x0u, 0x1000u, c.cap_len);
      space.add_standard_four();
      VirtioPciLayout layout;
      CHECK_STATUS(space.walk(layout), Status::malformed);
  } }

TEST(virtio_capability_walk, a_larger_cap_len_is_accepted)
{ ConfigSpace space;
  space.add(1u, 4u, 0x0u, 0x1000u, 32u);
  space.add(3u, 4u, 0x1000u, 0x1000u);
  space.add(4u, 4u, 0x2000u, 0x1000u);
  space.put32(space.add(2u, 4u, 0x3000u, 0x1000u, 20u) + 16u, 4u);
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::ok); }

TEST(virtio_capability_walk, regions_too_small_to_use_are_malformed)
{ struct Case { std::uint8_t type; std::uint32_t length; };
  const Case cases[] = {{1u, 55u}, {3u, 0u}, {2u, 1u}};
  for (const Case &c : cases) {
      ConfigSpace space;
      space.put32(space.add(c.type, 4u, 0x0u, c.length, c.type == 2u ? 20u : 16u) + 16u, 4u);
      space.add_standard_four();
      VirtioPciLayout layout;
      CHECK_STATUS(space.walk(layout), Status::malformed);
  } }

TEST(virtio_capability_walk, a_shared_memory_region_that_wraps_is_malformed)
{ ConfigSpace space;
  space.add_standard_four();
  const std::size_t shm = space.add(8u, 2u, 0x1000u, 0x0u, 24u, 1u);
  space.put32(shm + 16u, 0xFFFFFFFFu);
  space.put32(shm + 20u, 0x0u);
  space.put32(shm + 12u, 0xFFFFFFFFu);
  space.put32(shm + 20u, 0xFFFFFFFFu);
  VirtioPciLayout layout;
  CHECK_STATUS(space.walk(layout), Status::malformed); }

TEST(virtio_capability_walk, a_short_snapshot_is_malformed)
{ const FixtureFile fixture("virtio_gpu_pci_config.bin");
  VirtioPciLayout layout;
  CHECK_STATUS(VirtioCapabilityWalk(fixture.bytes(), 255u).walk(layout), Status::malformed);
  CHECK_STATUS(VirtioCapabilityWalk(nullptr, 256u).walk(layout), Status::malformed); }

TEST(virtio_capability_walk, a_rejected_walk_leaves_the_layout_unchanged)
{ const FixtureFile fixture("virtio_gpu_pci_config.bin");
  VirtioPciLayout layout;
  CHECK_STATUS(VirtioCapabilityWalk(fixture.bytes(), fixture.size()).walk(layout), Status::ok);
  ConfigSpace looping;
  looping.add_standard_four();
  looping.bytes()[0x41] = 0x40u;
  CHECK_STATUS(looping.walk(layout), Status::malformed);
  CHECK_EQ(layout.notify_multiplier(), 4u); }

TEST(virtio_capability_walk, survives_the_hostile_sweep_of_a_real_config_space)
{ const FixtureFile fixture("virtio_gpu_gl_pci_hostmem_config.bin");
  const av::HostileSweep sweep(rejects, fixture.bytes(), fixture.size());
  sweep.run();
  CHECK(sweep.cases_run() > 2000u); }
