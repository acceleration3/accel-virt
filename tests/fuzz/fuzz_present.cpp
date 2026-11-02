#include "miniport/ddi/dxgk_present_allocation_info_record.hpp"
#include "miniport/ddi/dxgkarg_present_record.hpp"
#include "miniport/present/present_request.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint32_t max_bytes = 512u;
constexpr std::uint32_t max_rects = 32u;
constexpr std::uint32_t max_patches = 3u;
constexpr std::uint32_t guard = 32u;
constexpr std::uint8_t  poison = 0x5Au;

av::RectRecord read_rect(av::WireReader &reader)
{
    const std::int32_t left = static_cast<std::int32_t>(reader.read_u32());
    const std::int32_t top = static_cast<std::int32_t>(reader.read_u32());
    const std::int32_t right = static_cast<std::int32_t>(reader.read_u32());
    const std::int32_t bottom = static_cast<std::int32_t>(reader.read_u32());
    return {left, top, right, bottom};
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader reader(data, size);
    std::uint8_t dma[max_bytes + guard];
    av::D3dddiPatchLocationListRecord patches[max_patches + 1u];
    av::RectRecord rects[max_rects] = {};
    av::DxgkPresentAllocationInfoRecord allocations[3] = {};
    std::memset(dma, poison, sizeof dma);
    std::memset(static_cast<void *>(patches), poison, sizeof patches);

    av::DxgkargPresentRecord request = {};
    request.DmaSize = reader.read_u16() % (max_bytes + 1u);
    request.PatchLocationListOutSize = reader.read_u8() % (max_patches + 1u);
    request.SubRectCnt = reader.read_u8() % (max_rects + 1u);
    request.MultipassOffset = reader.read_u8() % (max_rects + 2u);
    request.Flags = reader.read_u32();
    request.Color = reader.read_u32();
    request.FlipInterval = static_cast<std::int32_t>(reader.read_u32());
    const std::uint8_t nulls = reader.read_u8();
    request.SrcRect = read_rect(reader);
    request.DstRect = read_rect(reader);
    for (av::RectRecord &rect : rects) rect = read_rect(reader);
    for (av::DxgkPresentAllocationInfoRecord &allocation : allocations) allocation.AllocationVirtualAddress = reader.read_u64();
    request.pDmaBuffer = (nulls & 1u) != 0u ? nullptr : dma;
    request.pPatchLocationListOut = (nulls & 2u) != 0u ? nullptr : patches;
    request.pDstSubRects = (nulls & 4u) != 0u ? nullptr : rects;
    request.pAllocationList = (nulls & 8u) != 0u ? nullptr : allocations;
    const std::uint32_t patch_capacity = request.PatchLocationListOutSize;
    const std::uint32_t dma_capacity = request.DmaSize;

    av::MockPlatform platform;
    static_cast<void>(av::PresentRequest(platform, request).present());

    for (std::size_t byte = dma_capacity; byte < sizeof dma; byte++)
        if (dma[byte] != poison) __builtin_trap();
    const auto *patch_bytes = reinterpret_cast<const std::uint8_t *>(patches);
    for (std::size_t byte = sizeof(av::D3dddiPatchLocationListRecord) * patch_capacity; byte < sizeof patches; byte++)
        if (patch_bytes[byte] != poison) __builtin_trap();
    if (request.pDmaBuffer != nullptr &&
        static_cast<std::size_t>(static_cast<std::uint8_t *>(request.pDmaBuffer) - dma) > dma_capacity)
        __builtin_trap();
    if (request.pPatchLocationListOut != nullptr &&
        static_cast<std::size_t>(request.pPatchLocationListOut - patches) > patch_capacity)
        __builtin_trap();
    if (request.MultipassOffset > max_rects + 1u) __builtin_trap();
    if (platform.unclaimed_violations() != 0u) __builtin_trap();
    return 0;
}
