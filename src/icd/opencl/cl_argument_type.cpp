#include "icd/opencl/cl_argument_type.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

namespace {

struct ScalarName {
    const char *name_;
    uint32_t    bytes_;
};

constexpr ScalarName scalars[] = {{"uchar", 1u}, {"char", 1u},  {"ushort", 2u}, {"short", 2u}, {"half", 2u},
                                  {"uint", 4u},  {"int", 4u},   {"float", 4u},  {"ulong", 8u}, {"long", 8u},
                                  {"double", 8u}};

}

ClArgumentType::ClArgumentType(const char *type_name, cl_kernel_arg_address_qualifier qualifier)
    : name_(type_name != nullptr ? type_name : ""), qualifier_(qualifier), length_(0u)
{
    AV_SCOPE("ClArgumentType::ClArgumentType");
    while (name_[length_] != '\0') ++length_;
}

bool ClArgumentType::starts_with(const char *prefix) const
{
    AV_SCOPE("ClArgumentType::starts_with");
    uint32_t index = 0u;
    while (prefix[index] != '\0') {
        if (index >= length_ || name_[index] != prefix[index]) return false;
        ++index;
    }
    return true;
}

bool ClArgumentType::ends_with(const char *suffix) const
{
    AV_SCOPE("ClArgumentType::ends_with");
    uint32_t size = 0u;
    while (suffix[size] != '\0') ++size;
    if (size > length_) return false;
    for (uint32_t index = 0u; index < size; ++index)
        if (name_[length_ - size + index] != suffix[index]) return false;
    return true;
}

uint32_t ClArgumentType::scalar_bytes(uint32_t &letters) const
{
    AV_SCOPE("ClArgumentType::scalar_bytes");
    for (const ScalarName &scalar : scalars) {
        if (!starts_with(scalar.name_)) continue;
        letters = 0u;
        while (scalar.name_[letters] != '\0') ++letters;
        return scalar.bytes_;
    }
    return 0u;
}

ClKernelShape::ArgKind ClArgumentType::kind() const
{
    AV_SCOPE("ClArgumentType::kind");
    using Kind = ClKernelShape::ArgKind;
    if (length_ == 0u) return Kind::unknown;
    if (name_[length_ - 1u] == '*') {
        if (qualifier_ == CL_KERNEL_ARG_ADDRESS_LOCAL) return Kind::local;
        if (qualifier_ == CL_KERNEL_ARG_ADDRESS_GLOBAL || qualifier_ == CL_KERNEL_ARG_ADDRESS_CONSTANT)
            return Kind::buffer;
        return Kind::unknown;
    }
    if (starts_with("image") && ends_with("_t")) return Kind::image;
    if (starts_with("sampler_t") && length_ == 9u) return Kind::sampler;
    return bytes() != 0u ? Kind::value : Kind::unknown;
}

uint32_t ClArgumentType::bytes() const
{
    AV_SCOPE("ClArgumentType::bytes");
    uint32_t letters = 0u;
    const uint32_t base = scalar_bytes(letters);
    if (base == 0u) return 0u;
    uint32_t lanes = 0u;
    for (uint32_t at = letters; at < length_; ++at) {
        if (name_[at] < '0' || name_[at] > '9') return 0u;
        lanes = lanes * 10u + static_cast<uint32_t>(name_[at] - '0');
    }
    if (letters == length_) return base;
    if (lanes != 2u && lanes != 3u && lanes != 4u && lanes != 8u && lanes != 16u) return 0u;
    return base * (lanes == 3u ? 4u : lanes);
}

}
