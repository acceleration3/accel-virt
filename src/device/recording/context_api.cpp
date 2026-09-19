#include "device/recording/context_api.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ContextApi::ContextApi() : context_(0u), api_(unknown_api), known_(false)
{
    AV_SCOPE("ContextApi::ContextApi");
}

void ContextApi::learn(uint32_t context, uint16_t api)
{
    AV_SCOPE("ContextApi::learn");
    context_ = context;
    api_ = api;
    known_ = true;
}

bool ContextApi::known() const
{
    AV_SCOPE("ContextApi::known");
    return known_;
}

bool ContextApi::names(uint32_t context) const
{
    AV_SCOPE("ContextApi::names");
    return known_ && context_ == context;
}

uint16_t ContextApi::api() const
{
    AV_SCOPE("ContextApi::api");
    return api_;
}

bool ContextApi::invariants_hold() const
{
    AV_SCOPE("ContextApi::invariants_hold");
    return known_ || (context_ == 0u && api_ == unknown_api);
}

}  // namespace av
