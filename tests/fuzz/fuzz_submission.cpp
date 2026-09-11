/* Render and Patch over attacker-chosen sizes, offsets and indices, with
 * buffers exactly as large as the sizes they claim (a larger claim is a
 * dxgkrnl contract breach nothing can detect). Whatever is refused, nothing
 * may be written outside the DMA buffer or the output patch list. */
#include "../../src/core/ddi/d3dddi_patch_location_list_record.hpp"
#include "../../src/core/ddi/dxgk_allocation_list_record.hpp"
#include "../../src/core/ddi/dxgkarg_patch_record.hpp"
#include "../../src/core/ddi/dxgkarg_render_record.hpp"
#include "../../src/core/patch_request.hpp"
#include "../../src/core/render_request.hpp"
#include "../../src/proto/wire_reader.hpp"
#include "../harness/mock_platform.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint32_t max_bytes = 256u;
constexpr std::uint32_t max_entries = 8u;
constexpr std::uint32_t guard = 32u;
constexpr std::uint8_t  poison = 0x5Au;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader reader(data, size);
    std::uint8_t command[max_bytes] = {};
    std::uint8_t dma[max_bytes + guard];
    av::DxgkAllocationListRecord allocations[max_entries] = {};
    av::D3dddiPatchLocationListRecord in[max_entries] = {};
    av::D3dddiPatchLocationListRecord out[max_entries + 1u];
    std::memset(dma, poison, sizeof dma);
    std::memset(static_cast<void *>(out), poison, sizeof out);

    const std::uint32_t command_length = reader.read_u16() % (max_bytes + 1u);
    const std::uint32_t dma_size = reader.read_u16() % (max_bytes + 1u);
    const std::uint32_t allocation_count = reader.read_u8() % (max_entries + 1u);
    const std::uint32_t in_count = reader.read_u8() % (max_entries + 1u);
    const std::uint32_t out_count = reader.read_u8() % (max_entries + 1u);
    for (av::DxgkAllocationListRecord &allocation : allocations) allocation.PhysicalAddress = static_cast<std::int64_t>(reader.read_u64());
    for (av::D3dddiPatchLocationListRecord &location : in) {
        location.AllocationIndex = reader.read_u32();
        location.AllocationOffset = reader.read_u32();
        location.PatchOffset = reader.read_u32();
    }
    reader.copy_bytes(command, sizeof command);

    av::MockPlatform platform;
    av::DxgkargRenderRecord render = {};
    render.pCommand = command;
    render.CommandLength = command_length;
    render.pDmaBuffer = dma;
    render.DmaSize = dma_size;
    render.pAllocationList = allocations;
    render.AllocationListSize = allocation_count;
    render.pPatchLocationListIn = in;
    render.PatchLocationListInSize = in_count;
    render.pPatchLocationListOut = out;
    render.PatchLocationListOutSize = out_count;
    static_cast<void>(av::RenderRequest(platform, render).render());

    av::DxgkargPatchRecord patch = {};
    patch.pDmaBuffer = dma;
    patch.DmaBufferSize = dma_size;
    patch.DmaBufferSubmissionStartOffset = reader.read_u16();
    patch.DmaBufferSubmissionEndOffset = reader.read_u16();
    patch.pAllocationList = allocations;
    patch.AllocationListSize = allocation_count;
    patch.pPatchLocationList = in;
    patch.PatchLocationListSize = in_count;
    patch.PatchLocationListSubmissionStart = reader.read_u32();
    patch.PatchLocationListSubmissionLength = reader.read_u32();
    static_cast<void>(av::PatchRequest(platform, patch).patch());

    for (std::uint32_t byte = dma_size; byte < sizeof dma; byte++)
        if (dma[byte] != poison) __builtin_trap();
    const auto *out_bytes = reinterpret_cast<const std::uint8_t *>(out);
    for (std::size_t byte = sizeof(av::D3dddiPatchLocationListRecord) * out_count; byte < sizeof out; byte++)
        if (out_bytes[byte] != poison) __builtin_trap();
    if (platform.unclaimed_violations() != 0u) __builtin_trap();
    return 0;
}
