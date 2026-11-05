#include "icd/opengl/half_float.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

namespace {

constexpr uint32_t single_exponent_all = 0xFFu;
constexpr uint32_t half_exponent_all = 0x1Fu;
constexpr int32_t  exponent_rebias = 127 - 15;
constexpr uint32_t half_infinity = 0x7C00u;
constexpr uint32_t half_quiet_bit = 0x0200u;

}

HalfFloat::HalfFloat(uint16_t bits) : bits_(bits)
{
    AV_SCOPE("HalfFloat::HalfFloat");
}

HalfFloat::HalfFloat(float value) : bits_(0u)
{
    AV_SCOPE("HalfFloat::HalfFloat");
    uint32_t single = 0u;
    __builtin_memcpy(&single, &value, sizeof single);
    const uint32_t sign = (single >> 16u) & 0x8000u;
    const uint32_t exponent = (single >> 23u) & single_exponent_all;
    uint32_t       mantissa = single & 0x7FFFFFu;
    if (exponent == single_exponent_all) {
        bits_ = static_cast<uint16_t>(sign | half_infinity | (mantissa != 0u ? half_quiet_bit : 0u));
        return;
    }
    const int32_t rebased = static_cast<int32_t>(exponent) - exponent_rebias;
    if (rebased >= static_cast<int32_t>(half_exponent_all)) {
        bits_ = static_cast<uint16_t>(sign | half_infinity);
        return;
    }
    uint32_t half = 0u;
    uint32_t remainder = 0u;
    uint32_t midpoint = 0u;
    if (rebased <= 0) {
        if (rebased < -10) {
            bits_ = static_cast<uint16_t>(sign);
            return;
        }
        mantissa |= 0x800000u;
        const uint32_t shift = static_cast<uint32_t>(14 - rebased);
        half = mantissa >> shift;
        remainder = mantissa & ((uint32_t{1} << shift) - 1u);
        midpoint = uint32_t{1} << (shift - 1u);
    } else {
        half = (static_cast<uint32_t>(rebased) << 10u) | (mantissa >> 13u);
        remainder = mantissa & 0x1FFFu;
        midpoint = 0x1000u;
    }
    if (remainder > midpoint || (remainder == midpoint && (half & 1u) != 0u)) half++;
    bits_ = static_cast<uint16_t>(sign | half);
}

uint16_t HalfFloat::bits() const
{
    AV_SCOPE("HalfFloat::bits");
    return bits_;
}

float HalfFloat::value() const
{
    AV_SCOPE("HalfFloat::value");
    const uint32_t sign = (static_cast<uint32_t>(bits_) & 0x8000u) << 16u;
    const uint32_t exponent = (static_cast<uint32_t>(bits_) >> 10u) & half_exponent_all;
    uint32_t       mantissa = static_cast<uint32_t>(bits_) & 0x3FFu;
    uint32_t       single = 0u;
    if (exponent == half_exponent_all) {
        single = sign | 0x7F800000u | (mantissa << 13u);
    } else if (exponent != 0u) {
        single = sign | ((exponent + static_cast<uint32_t>(exponent_rebias)) << 23u) | (mantissa << 13u);
    } else if (mantissa != 0u) {
        uint32_t shifts = 0u;
        while ((mantissa & 0x400u) == 0u) {
            mantissa <<= 1u;
            shifts++;
        }
        single = sign | ((static_cast<uint32_t>(exponent_rebias) + 1u - shifts) << 23u) | ((mantissa & 0x3FFu) << 13u);
    } else {
        single = sign;
    }
    float result = 0.0f;
    __builtin_memcpy(&result, &single, sizeof result);
    return result;
}

}
