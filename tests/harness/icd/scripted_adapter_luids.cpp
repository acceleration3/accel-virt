#include "harness/icd/scripted_adapter_luids.hpp"

namespace av {

ScriptedAdapterLuids::ScriptedAdapterLuids() : luid_(default_luid), refusing_(false), asked_(0u) {}

Status ScriptedAdapterLuids::adapter_luid(std::uint32_t adapter, std::uint64_t &luid)
{
    asked_ = adapter;
    if (refusing_) return Status::not_found;
    luid = luid_;
    return Status::ok;
}

void          ScriptedAdapterLuids::answer(std::uint64_t luid) { luid_ = luid; }
void          ScriptedAdapterLuids::refuse() { refusing_ = true; }
std::uint32_t ScriptedAdapterLuids::asked() const { return asked_; }

}  // namespace av
