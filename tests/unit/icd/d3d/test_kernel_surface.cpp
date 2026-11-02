/* A presented or shared surface as the kernel sees it: its rows in pages for
 * the miniport, a primary's scanout, the D3DDDIFORMAT each presentable DXGI
 * format is scanned out as, an identity asked for and the one the miniport
 * mints read back as the key - and an open that finds that key only in what
 * this driver made. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/kernel_surface.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <cstdint>
#include <cstring>

using av::AllocationDescriptor;
using av::AllocationKind;
using av::ScanoutTarget;
using av::SharedObjectKind;
using av::SharedObjectRecord;
using av::Status;
using av::d3d::KernelSurface;

namespace {

constexpr std::uint32_t b8g8r8a8 = 87u;
constexpr std::uint32_t r32g32b32a32 = 2u;
constexpr std::uint32_t ddi_a8r8g8b8 = 21u;

AllocationDescriptor described(KernelSurface &surface)
{
    AllocationDescriptor descriptor;
    CHECK_STATUS(descriptor.decode(surface.allocation_data(), AllocationDescriptor::encoded_size), Status::ok);
    return descriptor;
}

/* What the miniport does at CreateAllocation: mint the id into the record. */
void mint(KernelSurface &surface, std::uint64_t id)
{
    std::uint8_t *const record_bytes = surface.allocation_data() + AllocationDescriptor::encoded_size;
    SharedObjectRecord record;
    CHECK_STATUS(record.decode(record_bytes, SharedObjectRecord::encoded_size), Status::ok);
    record.assign(id, 7u);
    CHECK_STATUS(record.encode(record_bytes, SharedObjectRecord::encoded_size), Status::ok);
}

}  // namespace

TEST(kernel_surface, a_b8g8r8a8_buffer_is_a_surface_of_its_rows_rounded_to_pages_asking_an_identity)
{ KernelSurface surface(100u, 10u, b8g8r8a8);
  CHECK_STATUS(surface.make(), Status::ok);
  CHECK_EQ(surface.pitch(), 400u);
  CHECK_EQ(surface.bytes(), 4096u);
  const AllocationDescriptor descriptor = described(surface);
  CHECK(descriptor.kind() == AllocationKind::surface);
  CHECK(descriptor.cpu_visible());
  CHECK_EQ(descriptor.layout().width(), 100u);
  CHECK_EQ(descriptor.layout().height(), 10u);
  CHECK_EQ(descriptor.layout().format(), ddi_a8r8g8b8);
  CHECK_EQ(descriptor.layout().pitch(), 400u);
  CHECK_EQ(descriptor.size(), 4096u);
  CHECK(descriptor.scanout().is_empty());
  SharedObjectRecord asked;
  CHECK_STATUS(asked.decode(surface.allocation_data() + AllocationDescriptor::encoded_size,
                            SharedObjectRecord::encoded_size),
               Status::ok);
  CHECK(!asked.assigned());
  CHECK_EQ(asked.kind().value(), SharedObjectKind::memory);
  CHECK_EQ(asked.size(), 4096u);
  /* Asked for, not minted: no key yet. */
  CHECK_EQ(surface.key(), 0u); }

TEST(kernel_surface, a_primary_is_described_with_its_scanout_even_one_naming_source_0_at_no_rate)
{ KernelSurface surface(64u, 64u, b8g8r8a8, ScanoutTarget(1u, 60u, 1u));
  CHECK_STATUS(surface.make(), Status::ok);
  const AllocationDescriptor descriptor = described(surface);
  CHECK(descriptor.kind() == AllocationKind::shared_primary);
  CHECK_EQ(descriptor.scanout().source(), 1u);
  CHECK_EQ(descriptor.scanout().refresh_numerator(), 60u);
  KernelSurface unrated(64u, 64u, b8g8r8a8, ScanoutTarget());
  CHECK_STATUS(unrated.make(), Status::ok);
  CHECK(described(unrated).kind() == AllocationKind::shared_primary); }

TEST(kernel_surface, the_identity_the_miniport_mints_is_the_key_and_what_an_open_of_it_finds)
{ KernelSurface made(64u, 32u, b8g8r8a8);
  CHECK_STATUS(made.make(), Status::ok);
  mint(made, 0x1234u);
  CHECK_EQ(made.key(), 0x1234u);
  KernelSurface opened;
  CHECK_STATUS(opened.open(made.resource_data(), KernelSurface::resource_bytes, made.allocation_data(),
                           KernelSurface::allocation_bytes),
               Status::ok);
  CHECK_EQ(opened.key(), 0x1234u);
  /* Opened before the miniport minted anything: nothing to find by. */
  KernelSurface unminted(64u, 32u, b8g8r8a8);
  CHECK_STATUS(unminted.make(), Status::ok);
  KernelSurface early;
  CHECK_STATUS(early.open(unminted.resource_data(), KernelSurface::resource_bytes, unminted.allocation_data(),
                          KernelSurface::allocation_bytes),
               Status::ok);
  CHECK_EQ(early.key(), 0u); }

TEST(kernel_surface, each_presentable_format_is_scanned_out_as_its_kernel_format)
{ struct Case { std::uint32_t dxgi, ddi; };
  const Case cases[] = {{87u, 21u}, {91u, 21u}, {90u, 21u}, {88u, 22u}, {93u, 22u}, {92u, 22u},
                        {24u, 31u}, {23u, 31u}, {10u, 113u}, {28u, 32u}};
  for (const Case c : cases) {
      KernelSurface surface(16u, 16u, c.dxgi);
      CHECK_STATUS(surface.make(), Status::ok);
      CHECK_EQ(described(surface).layout().format(), c.ddi);
  } }

TEST(kernel_surface, a_format_with_no_fixed_size_is_laid_out_as_a_32_bit_one)
{ KernelSurface surface(10u, 1u, 0u);
  CHECK_EQ(surface.pitch(), 40u);
  KernelSurface wide(10u, 1u, r32g32b32a32);
  CHECK_EQ(wide.pitch(), 160u); }

/* A block-compressed surface is rows of blocks: a 2048x2048 BC1 texture is
 * 512 rows of 512 eight-byte blocks, 2 MiB - not 32 MiB at a block a texel -
 * and a BC3 surface smaller than its blocks rounds up to whole ones. */
TEST(kernel_surface, a_block_compressed_surface_is_sized_in_rows_of_blocks)
{ KernelSurface bc1(2048u, 2048u, 71u);
  CHECK_EQ(bc1.pitch(), 4096u);
  CHECK_EQ(bc1.bytes(), 2u * 1024u * 1024u);
  CHECK_STATUS(bc1.make(), Status::ok);
  const AllocationDescriptor kernel_view = described(bc1);   /* decodes as the miniport does: rows x pitch fits */
  CHECK_EQ(kernel_view.layout().height(), 512u);
  CHECK(kernel_view.layout().bytes() <= kernel_view.size());
  KernelSurface bc3(6u, 6u, 77u);
  CHECK_EQ(bc3.pitch(), 32u);
  CHECK_EQ(bc3.bytes(), 4096u);
  KernelSurface tall(4u, 4096u * 4u + 1u, 71u);   /* one block wide: 4097 rows of 8 bytes, nine pages */
  CHECK_EQ(tall.pitch(), 8u);
  CHECK_EQ(tall.bytes(), 9u * 4096u); }

TEST(kernel_surface, a_surface_of_no_texels_or_a_row_past_32_bits_of_bytes_is_refused)
{ KernelSurface no_width(0u, 16u, b8g8r8a8);
  CHECK_STATUS(no_width.make(), Status::invalid_parameter);
  KernelSurface no_height(16u, 0u, b8g8r8a8);
  CHECK_STATUS(no_height.make(), Status::invalid_parameter);
  /* 2^28 texels of 16 bytes is a row of exactly 2^32 bytes. */
  KernelSurface too_wide(1u << 28u, 1u, r32g32b32a32);
  CHECK_EQ(too_wide.pitch(), 0u);
  CHECK_STATUS(too_wide.make(), Status::invalid_parameter);
  /* One byte less wide fits. */
  KernelSurface widest((1u << 28u) - 1u, 1u, r32g32b32a32);
  CHECK_EQ(widest.pitch(), 0xfffffff0u); }

TEST(kernel_surface, an_open_of_anything_but_this_driver_s_shared_surface_finds_nothing)
{ KernelSurface made(16u, 16u, b8g8r8a8);
  CHECK_STATUS(made.make(), Status::ok);
  mint(made, 0x99u);
  const void *const resource = made.resource_data();
  const void *const allocation = made.allocation_data();
  KernelSurface opened;
  CHECK_STATUS(opened.open(nullptr, KernelSurface::resource_bytes, allocation, KernelSurface::allocation_bytes),
               Status::not_found);
  CHECK_STATUS(opened.open(resource, KernelSurface::resource_bytes - 1u, allocation, KernelSurface::allocation_bytes),
               Status::not_found);
  std::uint64_t foreign[2] = {0x4e4f54u, 0u};
  CHECK_STATUS(opened.open(foreign, sizeof foreign, allocation, KernelSurface::allocation_bytes), Status::not_found);
  CHECK_STATUS(opened.open(resource, KernelSurface::resource_bytes, nullptr, KernelSurface::allocation_bytes),
               Status::malformed);
  CHECK_STATUS(opened.open(resource, KernelSurface::resource_bytes, allocation, KernelSurface::allocation_bytes - 1u),
               Status::malformed);
  CHECK_EQ(opened.key(), 0u);
  /* This driver's mark over an identity that is no record: no key. */
  std::uint8_t garbage[KernelSurface::allocation_bytes];
  std::memset(garbage, 0xff, sizeof garbage);
  CHECK_STATUS(opened.open(resource, KernelSurface::resource_bytes, garbage, sizeof garbage), Status::malformed);
  CHECK_EQ(opened.key(), 0u); }

/* The allocation of a primary is scanned out at its source; any other
 * surface is no primary and names source 0. */
TEST(kernel_surface, a_primary_says_where_it_is_scanned_out_and_any_other_is_none)
{ const KernelSurface primary(64u, 64u, b8g8r8a8, ScanoutTarget(2u, 60u, 1u));
  CHECK(primary.primary());
  CHECK_EQ(primary.source(), 2u);
  const KernelSurface plain(64u, 64u, b8g8r8a8);
  CHECK(!plain.primary());
  CHECK_EQ(plain.source(), 0u); }
