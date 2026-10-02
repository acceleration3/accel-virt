/* A pointer shape with attacker-chosen flags, size, pitch and hot spot over a
 * bitmap exactly as large as that shape claims. Nothing may be read past the
 * bitmap, nothing written past the 64x64 image, and a refused shape leaves the
 * image untouched. */
#include "miniport/ddi/dxgkarg_set_pointer_shape_record.hpp"
#include "miniport/display/cursor_image.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint32_t max_pitch = 300u;
constexpr std::uint32_t guard = 64u;
constexpr std::uint8_t  poison = 0x5Au;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader reader(data, size);
    av::DxgkargSetPointerShapeRecord shape = {};
    shape.Flags = reader.read_u8();
    shape.Width = reader.read_u8();
    shape.Height = reader.read_u8();
    shape.Pitch = reader.read_u16() % (max_pitch + 1u);
    shape.XHot = reader.read_u8();
    shape.YHot = reader.read_u8();
    /* What the shape claims: two masks for monochrome, one bitmap otherwise. */
    const std::size_t claimed = std::size_t{shape.Pitch} * shape.Height * (shape.Flags == 1u ? 2u : 1u);
    auto *const bitmap = static_cast<std::uint8_t *>(std::malloc(claimed != 0u ? claimed : 1u));
    if (bitmap == nullptr) return 0;
    for (std::size_t byte = 0u; byte < claimed; byte++) bitmap[byte] = reader.read_u8();
    shape.pPixels = claimed != 0u ? bitmap : nullptr;

    std::uint8_t image[av::CursorImage::pixel_bytes + guard];
    std::memset(image, poison, sizeof image);
    av::MockPlatform platform;
    const av::Status status = av::CursorImage(platform, shape).draw(image);
    std::free(bitmap);

    for (std::size_t byte = av::CursorImage::pixel_bytes; byte < sizeof image; byte++)
        if (image[byte] != poison) __builtin_trap();
    if (status != av::Status::ok && image[0] != poison) __builtin_trap();
    if (platform.unclaimed_violations() != 0u) __builtin_trap();
    return 0;
}
