#include "executor/shim/scrgb_rows.hpp"

#include "core/trace/function_scope.hpp"

#include <math.h>
#include <string.h>

namespace av {

namespace {

constexpr uint32_t dxgi_r32g32b32a32_float = 2u;
constexpr uint32_t dxgi_r32g32b32_float = 6u;
constexpr uint32_t dxgi_r16g16b16a16_float = 10u;
constexpr uint32_t dxgi_r16g16_float = 34u;
constexpr uint32_t dxgi_r8g8b8a8_unorm = 28u;
constexpr uint32_t dxgi_b8g8r8a8_unorm = 87u;
constexpr uint32_t dxgi_b8g8r8x8_unorm = 88u;
constexpr float    srgb_knee = 0.0031308f;
constexpr float    srgb_linear_slope = 12.92f;
constexpr float    srgb_curve_scale = 1.055f;
constexpr float    srgb_curve_offset = 0.055f;
constexpr float    srgb_curve_exponent = 1.0f / 2.4f;
constexpr float    channel_peak = 255.0f;
constexpr float    half_denormal_scale = 1.0f / 16777216.0f;

float srgb_of(float linear)
{
    AV_SCOPE("srgb_of");
    if (linear <= srgb_knee) return linear * srgb_linear_slope;
    return srgb_curve_scale * powf(linear, srgb_curve_exponent) - srgb_curve_offset;
}

float half_to_float(uint16_t bits)
{
    AV_SCOPE("half_to_float");
    const uint32_t sign = (bits & 0x8000u) << 16u;
    const uint32_t exponent = (bits >> 10u) & 0x1Fu;
    const uint32_t fraction = bits & 0x3FFu;
    uint32_t       word = 0u;
    if (exponent == 0u) {
        const float magnitude = static_cast<float>(fraction) * half_denormal_scale;
        return sign != 0u ? -magnitude : magnitude;
    }
    if (exponent == 0x1Fu) word = sign | 0x7F800000u | (fraction << 13u);
    else word = sign | ((exponent + 112u) << 23u) | (fraction << 13u);
    float value = 0.0f;
    memcpy(&value, &word, sizeof(value));
    return value;
}

}

ScrgbRows::ScrgbRows() : table_{}, component_bytes_(0u), components_(0u), red_first_(false)
{
    AV_SCOPE("ScrgbRows::ScrgbRows");
    for (uint32_t step = 0u; step <= table_steps; step++)
        table_[step] = srgb_of(static_cast<float>(step) / static_cast<float>(table_steps));
}

bool ScrgbRows::use(uint32_t source_format, uint32_t target_format)
{
    AV_SCOPE("ScrgbRows::use");
    component_bytes_ = 0u;
    components_ = 0u;
    switch (source_format) {
    case dxgi_r32g32b32a32_float: component_bytes_ = 4u; components_ = 4u; break;
    case dxgi_r32g32b32_float: component_bytes_ = 4u; components_ = 3u; break;
    case dxgi_r16g16b16a16_float: component_bytes_ = 2u; components_ = 4u; break;
    case dxgi_r16g16_float: component_bytes_ = 2u; components_ = 2u; break;
    default: return false;
    }
    red_first_ = target_format == dxgi_r8g8b8a8_unorm;
    if (red_first_ || target_format == dxgi_b8g8r8a8_unorm || target_format == dxgi_b8g8r8x8_unorm) return true;
    component_bytes_ = 0u;
    components_ = 0u;
    return false;
}

uint32_t ScrgbRows::pixel_bytes() const
{
    AV_SCOPE("ScrgbRows::pixel_bytes");
    return component_bytes_ * components_;
}

uint8_t ScrgbRows::encode(float linear) const
{
    AV_SCOPE("ScrgbRows::encode");
    if (!(linear > 0.0f)) return 0u;
    if (linear >= 1.0f) return 255u;
    const float    position = linear * static_cast<float>(table_steps);
    const uint32_t step = static_cast<uint32_t>(position);
    const float    fraction = position - static_cast<float>(step);
    const float    encoded = table_[step] + (table_[step + 1u] - table_[step]) * fraction;
    return static_cast<uint8_t>(encoded * channel_peak + 0.5f);
}

void ScrgbRows::convert(const uint8_t *from, uint32_t pixels, uint8_t *to) const
{
    AV_SCOPE("ScrgbRows::convert");
    const uint32_t stride = pixel_bytes();
    for (uint32_t pixel = 0u; pixel < pixels; pixel++) {
        float          channel[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        const uint8_t *at = from + static_cast<size_t>(pixel) * stride;
        for (uint32_t index = 0u; index < components_; index++) {
            if (component_bytes_ == 2u) {
                uint16_t bits = 0u;
                memcpy(&bits, at + index * 2u, sizeof(bits));
                channel[index] = half_to_float(bits);
            } else {
                memcpy(&channel[index], at + index * 4u, sizeof(float));
            }
        }
        uint8_t *const out = to + static_cast<size_t>(pixel) * 4u;
        const uint32_t red = red_first_ ? 0u : 2u;
        out[red] = encode(channel[0]);
        out[1] = encode(channel[1]);
        out[2u - red] = encode(channel[2]);
        out[3] = !(channel[3] > 0.0f) ? 0u : channel[3] >= 1.0f ? 255u
                 : static_cast<uint8_t>(channel[3] * channel_peak + 0.5f);
    }
}

}
