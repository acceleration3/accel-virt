#include "miniport/ddi/dxgk_segment_descriptor4_record.hpp"
#include "miniport/memory/local_memory_segment.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct LocalMemorySegmentProbe {
    static void set_size(LocalMemorySegment &segment, std::uint64_t size) { segment.size_ = size; }
};

}

using av::CapsetInfo;
using av::DxgkSegmentDescriptor4Record;
using av::GpuIdentity;
using av::Irql;
using av::LocalMemorySegment;
using av::LocalMemorySegmentProbe;
using av::MockPlatform;
using av::ProtocolVersion;
using av::Violation;

namespace {
constexpr std::uint64_t radeon_local_memory = 17095983104ull;
constexpr std::uint8_t  uuid[GpuIdentity::uuid_bytes] = {7};

CapsetInfo capset(std::uint64_t local_memory)
{
    return CapsetInfo(ProtocolVersion(ProtocolVersion::current_major, ProtocolVersion::current_minor), 1u, 64u,
                      12u, 24u, 0u,
                      GpuIdentity(0x1002u, 0x7550u, GpuIdentity::discrete_gpu, uuid, local_memory, 32ull << 30,
                                  "the host's"));
}

}

TEST(local_memory_segment_offer, is_the_host_gpu_s_local_memory_rounded_down_to_a_page)
{ MockPlatform platform; LocalMemorySegment segment(platform);
  CHECK(!segment.present());
  segment.offer(capset(radeon_local_memory + 0x123u));
  CHECK(segment.present());
  CHECK(segment.size() == radeon_local_memory);
  CHECK(segment.invariants_hold()); }

TEST(local_memory_segment_offer, less_than_a_page_is_no_segment)
{ MockPlatform platform; LocalMemorySegment segment(platform);
  segment.offer(capset(0xFFFu));
  CHECK(!segment.present());
  CHECK(segment.size() == 0u);
  segment.offer(capset(0u));
  CHECK(!segment.present()); }

TEST(local_memory_segment_forget, leaves_no_segment_until_the_next_offer)
{ MockPlatform platform; LocalMemorySegment segment(platform);
  segment.offer(capset(radeon_local_memory));
  segment.forget();
  CHECK(!segment.present());
  CHECK(segment.size() == 0u);
  segment.offer(capset(std::uint64_t{8} << 30));
  CHECK(segment.size() == std::uint64_t{8} << 30); }

TEST(local_memory_segment_describe, is_a_plain_memory_segment_at_the_base_it_is_given)
{ MockPlatform platform; LocalMemorySegment segment(platform);
  segment.offer(capset(radeon_local_memory));
  DxgkSegmentDescriptor4Record descriptor;
  std::memset(&descriptor, 0xAB, sizeof descriptor);
  segment.describe(descriptor, 0x1C0000000);
  CHECK_EQ(descriptor.Flags, 0u);
  CHECK(descriptor.BaseAddress == 0x1C0000000);
  CHECK(descriptor.Size == radeon_local_memory);
  CHECK(descriptor.CommitLimit == radeon_local_memory);
  DxgkSegmentDescriptor4Record cleared = {};
  cleared.BaseAddress = 0x1C0000000;
  cleared.Size = radeon_local_memory;
  cleared.CommitLimit = radeon_local_memory;
  CHECK(std::memcmp(&descriptor, &cleared, sizeof descriptor) == 0); }

TEST(local_memory_segment_offer, and_forget_are_passive_only)
{ MockPlatform platform; LocalMemorySegment segment(platform);
  platform.set_irql(Irql::dispatch);
  segment.offer(capset(radeon_local_memory));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::dispatch);
  segment.forget();
  CHECK(!segment.present());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }

TEST(local_memory_segment_invariants, hold_only_for_whole_pages)
{ MockPlatform platform; LocalMemorySegment segment(platform);
  CHECK(segment.invariants_hold());
  LocalMemorySegmentProbe::set_size(segment, 0x1001u);
  CHECK(!segment.invariants_hold());
  LocalMemorySegmentProbe::set_size(segment, 0x2000u);
  CHECK(segment.invariants_hold()); }
