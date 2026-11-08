#include "executor/shim/conversion_target.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t dxgi_r16g16b16a16_typeless = 9u;
constexpr uint32_t dxgi_r16g16b16a16_float = 10u;
constexpr uint32_t dxgi_r10g10b10a2_typeless = 23u;
constexpr uint32_t dxgi_r10g10b10a2_unorm = 24u;
constexpr uint32_t dxgi_r8g8b8a8_typeless = 27u;
constexpr uint32_t dxgi_r8g8b8a8_unorm = 28u;
constexpr uint32_t dxgi_r8g8b8a8_srgb = 29u;
constexpr uint32_t dxgi_b8g8r8a8_unorm = 87u;
constexpr uint32_t dxgi_b8g8r8x8_unorm = 88u;
constexpr uint32_t dxgi_b8g8r8a8_typeless = 90u;
constexpr uint32_t dxgi_b8g8r8a8_srgb = 91u;
constexpr uint32_t dxgi_b8g8r8x8_typeless = 92u;
constexpr uint32_t dxgi_b8g8r8x8_srgb = 93u;
constexpr uint32_t dxgi_r32g32b32a32_float = 2u;
constexpr uint32_t dxgi_r32g32b32_float = 6u;
constexpr uint32_t dxgi_r16g16_float = 34u;

uint32_t unorm_of(uint32_t format)
{
    AV_SCOPE("unorm_of");
    switch (format) {
    case dxgi_r16g16b16a16_typeless: return dxgi_r16g16b16a16_float;
    case dxgi_r10g10b10a2_typeless: return dxgi_r10g10b10a2_unorm;
    case dxgi_r8g8b8a8_typeless:
    case dxgi_r8g8b8a8_srgb: return dxgi_r8g8b8a8_unorm;
    case dxgi_b8g8r8a8_typeless:
    case dxgi_b8g8r8a8_srgb: return dxgi_b8g8r8a8_unorm;
    case dxgi_b8g8r8x8_typeless:
    case dxgi_b8g8r8x8_srgb: return dxgi_b8g8r8x8_unorm;
    default: return format;
    }
}

uint32_t srgb_of(uint32_t format)
{
    AV_SCOPE("srgb_of");
    switch (unorm_of(format)) {
    case dxgi_r8g8b8a8_unorm: return dxgi_r8g8b8a8_srgb;
    case dxgi_b8g8r8a8_unorm: return dxgi_b8g8r8a8_srgb;
    case dxgi_b8g8r8x8_unorm: return dxgi_b8g8r8x8_srgb;
    default: return format;
    }
}

bool encoded(uint32_t format)
{
    AV_SCOPE("encoded");
    return format == dxgi_r8g8b8a8_srgb || format == dxgi_b8g8r8a8_srgb || format == dxgi_b8g8r8x8_srgb;
}

bool floating(uint32_t format)
{
    AV_SCOPE("floating");
    return format == dxgi_r16g16b16a16_float || format == dxgi_r32g32b32a32_float || format == dxgi_r32g32b32_float ||
           format == dxgi_r16g16_float;
}

bool drawable(uint32_t format)
{
    AV_SCOPE("drawable");
    const uint32_t base = unorm_of(format);
    return base == dxgi_r8g8b8a8_unorm || base == dxgi_b8g8r8a8_unorm || base == dxgi_b8g8r8x8_unorm ||
           base == dxgi_r10g10b10a2_unorm || base == dxgi_r16g16b16a16_float;
}

}

ConversionTarget::ConversionTarget(uint32_t source, uint32_t destination) : source_(source), destination_(destination)
{
    AV_SCOPE("ConversionTarget::ConversionTarget");
}

bool ConversionTarget::raw_copy() const
{
    AV_SCOPE("ConversionTarget::raw_copy");
    return unorm_of(source_) == unorm_of(destination_);
}

bool ConversionTarget::convertible() const
{
    AV_SCOPE("ConversionTarget::convertible");
    return drawable(destination_) && (drawable(source_) || floating(source_));
}

bool ConversionTarget::packed_bytes() const
{
    AV_SCOPE("ConversionTarget::packed_bytes");
    const uint32_t base = unorm_of(source_);
    return base == dxgi_r8g8b8a8_unorm || base == dxgi_b8g8r8a8_unorm || base == dxgi_b8g8r8x8_unorm;
}

uint32_t ConversionTarget::format() const
{
    AV_SCOPE("ConversionTarget::format");
    if (encoded(destination_)) return destination_;
    const uint32_t base = unorm_of(destination_);
    return encoded(source_) ? srgb_of(base) : base;
}

}
