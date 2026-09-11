/* The VidPn DDIs over an attacker-shaped VidPn: paths to any source and target
 * with any transformation, mode sets seeded with any sizes and formats, any
 * pivot, one dxgkrnl callback refused, and any descriptor window. Whatever is
 * refused, every path, mode set and mode info must be released, nothing written
 * past the descriptor buffer, and no IRQL rule broken. */
#include "../../src/core/cofunc_modality.hpp"
#include "../../src/core/ddi/dxgk_device_descriptor_record.hpp"
#include "../../src/core/ddi/dxgkarg_commit_vidpn_record.hpp"
#include "../../src/core/ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "../../src/core/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "../../src/core/ddi/dxgkarg_recommend_monitor_modes_record.hpp"
#include "../../src/core/display_mode_list.hpp"
#include "../../src/core/display_sources.hpp"
#include "../../src/core/edid.hpp"
#include "../../src/core/monitor_modes.hpp"
#include "../../src/core/vidpn_commit.hpp"
#include "../../src/core/vidpn_support_check.hpp"
#include "../../src/proto/wire_reader.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/vidpn_fixture.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint32_t max_paths = 3u;
constexpr std::uint32_t max_seeds = 3u;
constexpr std::uint32_t guard = 32u;
constexpr std::uint8_t  poison = 0x5Au;
constexpr std::int32_t  no_memory = static_cast<std::int32_t>(0xC0000017u);
const std::int32_t      formats[] = {22, 21, 0};

void shape(av::WireReader &reader, av::FakeVidPn &vidpn)
{
    const std::uint32_t paths = reader.read_u8() % (max_paths + 1u);
    for (std::uint32_t index = 0u; index < paths; index++) {
        const std::uint32_t source = reader.read_u8() % 3u;
        const std::uint32_t target = reader.read_u8() % 3u;
        av::D3dkmdtVidPnPresentPathRecord &path = vidpn.add_path(source, target);
        path.ContentTransformationScaling = reader.read_u8() % 3;
        path.ContentTransformationRotation = reader.read_u8() % 3;
    }
    const std::uint32_t seeds = reader.read_u8() % (max_seeds + 1u);
    for (std::uint32_t index = 0u; index < seeds; index++) {
        av::D3dkmdtVidPnSourceModeRecord source = {};
        source.PrimSurfSizeCx = reader.read_u16();
        source.PrimSurfSizeCy = reader.read_u16();
        source.PixelFormat = formats[reader.read_u8() % 3u];
        vidpn.source_modes(0u).seed(source, (reader.read_u8() & 1u) != 0u);
        av::D3dkmdtVidPnTargetModeRecord target = {};
        target.VideoSignalInfo.ActiveSizeCx = reader.read_u16();
        target.VideoSignalInfo.ActiveSizeCy = reader.read_u16();
        vidpn.target_modes(0u).seed(target, (reader.read_u8() & 1u) != 0u);
    }
    const std::uint8_t refused = reader.read_u8();
    if (refused < static_cast<std::uint8_t>(av::VidPnCall::count))
        vidpn.refuse(static_cast<av::VidPnCall>(refused), (reader.read_u8() & 1u) != 0u ? no_memory : 0);
}

void drive(av::WireReader &reader)
{
    av::VidPnFixture      fixture;
    const av::DisplayModeList modes;
    av::DisplaySources    sources(fixture.platform());
    shape(reader, fixture.vidpn());

    av::DxgkargIsSupportedVidPnRecord supported = {fixture.handle(), 0u, {}};
    static_cast<void>(av::VidPnSupportCheck(fixture.platform(), fixture.dxgkrnl(), supported).decide());

    const std::int32_t  pivot = reader.read_u8() % 7;
    const std::uint32_t pivot_source = reader.read_u8() % 3u;
    const std::uint32_t pivot_target = reader.read_u8() % 3u;
    const av::DxgkargEnumVidPnCofuncModalityRecord cofunc = {fixture.handle(), pivot, pivot_source, pivot_target, 0u};
    static_cast<void>(av::CofuncModality(fixture.platform(), fixture.dxgkrnl(), modes, cofunc).enumerate());

    const std::uint32_t commit_source = reader.read_u8() % 2u;
    const std::uint32_t commit_flags = reader.read_u8() & 3u;
    const av::DxgkargCommitVidPnRecord commit = {fixture.handle(), commit_source, 0, nullptr, commit_flags, 0u};
    static_cast<void>(av::VidPnCommit(fixture.platform(), fixture.dxgkrnl(), modes, sources, commit).apply());

    const av::DxgkargRecommendMonitorModesRecord monitor = {reader.read_u8() % 2u, 0u, fixture.vidpn().monitor_mode_set(),
                                                            &fixture.vidpn().monitor_interface()};
    static_cast<void>(av::MonitorModes(fixture.platform(), modes, monitor).recommend());

    std::uint8_t edid[av::Edid::block_bytes + guard];
    std::memset(edid, poison, sizeof edid);
    const std::uint32_t offset = reader.read_u8();
    const std::uint32_t length = reader.read_u8() % (av::Edid::block_bytes + 1u);
    av::DxgkDeviceDescriptorRecord descriptor = {offset, length, edid};
    const std::uint32_t window = descriptor.DescriptorLength;
    static_cast<void>(av::Edid(fixture.platform(), modes.preferred()).copy(descriptor));
    for (std::uint32_t byte = window; byte < sizeof edid; byte++)
        if (edid[byte] != poison) __builtin_trap();
    if (fixture.platform().unclaimed_violations() != 0u) __builtin_trap();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader reader(data, size);
    drive(reader);
    if (av::TestRegistry::instance().take_current_failure()) __builtin_trap();
    return 0;
}
