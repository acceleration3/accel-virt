#include "executor/opencl/cl_platform_choice.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {
constexpr const char *translated[] = {"rusticl", "Clover", "Portable Computing Language"};

constexpr uint32_t vendor_rank = 1u;
constexpr uint32_t translated_rank = 0u;

bool same(const char *name, const char *wanted)
{
    AV_SCOPE("same");
    size_t at = 0u;
    for (; wanted[at] != '\0'; ++at) {
        if (name[at] != wanted[at]) return false;
    }
    return name[at] == '\0';
}

uint32_t rank_of(const char *name)
{
    AV_SCOPE("rank_of");
    for (const char *one : translated) {
        if (same(name, one)) return translated_rank;
    }
    return vendor_rank;
}

}

ClPlatformChoice::ClPlatformChoice() : platform_(nullptr), device_(nullptr), rank_(0u), name_{}
{
    AV_SCOPE("ClPlatformChoice::ClPlatformChoice");
}

bool ClPlatformChoice::better(uint32_t rank, const char *name) const
{
    AV_SCOPE("ClPlatformChoice::better");
    if (device_ == nullptr) return true;
    if (rank != rank_) return rank > rank_;
    for (size_t at = 0u; at + 1u < name_bytes; ++at) {
        const unsigned char ours = static_cast<unsigned char>(name[at]);
        const unsigned char kept = static_cast<unsigned char>(name_[at]);
        if (ours != kept) return ours < kept;
        if (ours == '\0') return false;
    }
    return false;
}

void ClPlatformChoice::consider(void *platform, void *device, const char *name)
{
    AV_SCOPE("ClPlatformChoice::consider");
    if (platform == nullptr || device == nullptr) return;
    const char *const spelled = name != nullptr ? name : "";
    const uint32_t    rank = rank_of(spelled);
    if (!better(rank, spelled)) return;
    platform_ = platform;
    device_ = device;
    rank_ = rank;
    size_t at = 0u;
    for (; at + 1u < name_bytes && spelled[at] != '\0'; ++at) name_[at] = spelled[at];
    name_[at] = '\0';
}

void *ClPlatformChoice::platform() const
{
    AV_SCOPE("ClPlatformChoice::platform");
    return platform_;
}

void *ClPlatformChoice::device() const
{
    AV_SCOPE("ClPlatformChoice::device");
    return device_;
}

}
