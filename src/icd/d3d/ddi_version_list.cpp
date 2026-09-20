#include "icd/d3d/ddi_version_list.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {
namespace {

/* D3DWDDM3_2_DDI_SUPPORTED: interface (11 << 16) | 45, build 1
 * (d3d10umddi.h:7969-7974). The one WDDM level this project declares. */
constexpr uint64_t d3d11_wddm3_2 = (((11ull << 16) | 45ull) << 32) | (1ull << 16);

/* D3D12DDI_SUPPORTED_0110: interface (12 << 16) | 80 (R8), build 110
 * (d3d12umddi.h:10148, 13656). The newest revision the pinned kit declares. */
constexpr uint64_t d3d12_r8_0110 = (((12ull << 16) | 80ull) << 32) | (110ull << 16);

}  // namespace

DdiVersionList::DdiVersionList(RuntimeKind runtime) : words_{}, count_(1u)
{
    AV_SCOPE("DdiVersionList::DdiVersionList");
    words_[0] = runtime == RuntimeKind::d3d10_11 ? d3d11_wddm3_2 : d3d12_r8_0110;
}

uint32_t DdiVersionList::count() const
{
    AV_SCOPE("DdiVersionList::count");
    return count_;
}

uint64_t DdiVersionList::word(uint32_t index) const
{
    AV_SCOPE("DdiVersionList::word");
    if (index >= count_) return 0ull;
    return words_[index];
}

bool DdiVersionList::serves(uint32_t interface_version) const
{
    AV_SCOPE("DdiVersionList::serves");
    for (uint32_t index = 0u; index < count_; index++)
        if (static_cast<uint32_t>(words_[index] >> 32) == interface_version) return true;
    return false;
}

Hresult DdiVersionList::answer(uint32_t *entries, uint64_t *words) const
{
    AV_SCOPE("DdiVersionList::answer");
    if (entries == nullptr) return hr_invalid_arg;
    const uint32_t room = *entries;
    *entries = count_;
    if (words == nullptr) return hr_ok;
    if (room < count_) return hr_out_of_memory;
    for (uint32_t index = 0u; index < count_; index++) words[index] = words_[index];
    return hr_ok;
}

bool DdiVersionList::invariants_hold() const
{
    AV_SCOPE("DdiVersionList::invariants_hold");
    if (count_ == 0u || count_ > max_words) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        if (words_[index] == 0ull) return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (words_[earlier] == words_[index]) return false;
    }
    return true;
}

}  // namespace av::d3d
