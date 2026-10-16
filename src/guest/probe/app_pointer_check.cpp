#include "guest/probe/app_pointer_check.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

namespace {

/* Every byte of a page different from its neighbours and from the same byte
 * of the next page, so a page copied to the wrong place, or a stale one, is
 * a mismatch. */
uint8_t pattern_byte(uint32_t index)
{
    AV_SCOPE("pattern_byte");
    return static_cast<uint8_t>((index * 31u + 7u) ^ (index >> 12u));
}

}  // namespace

AppPointerCheck::AppPointerCheck() : pattern_{}, copied_{}
{
    AV_SCOPE("AppPointerCheck::AppPointerCheck");
    for (uint32_t index = 0u; index < bytes; index++) pattern_[index] = pattern_byte(index);
}

const uint8_t *AppPointerCheck::pattern() const
{
    AV_SCOPE("AppPointerCheck::pattern");
    return pattern_;
}

uint8_t *AppPointerCheck::copied()
{
    AV_SCOPE("AppPointerCheck::copied");
    return copied_;
}

void AppPointerCheck::judge(ScenarioSteps &steps, const char *const names[3], int32_t result, uint32_t aliased)
{
    AV_SCOPE("AppPointerCheck::judge");
    if (steps.check_result(names[0], result) != Status::ok) return;
    steps.check_measured(names[1], aliased == 1u ? Status::ok : Status::invalid_state, aliased);
    const uint32_t right = matching();
    steps.check_measured(names[2], right == bytes ? Status::ok : Status::invalid_state, right);
}

uint32_t AppPointerCheck::matching() const
{
    AV_SCOPE("AppPointerCheck::matching");
    uint32_t right = 0u;
    for (uint32_t index = 0u; index < bytes; index++) right += copied_[index] == pattern_[index] ? 1u : 0u;
    return right;
}

bool AppPointerCheck::invariants_hold() const
{
    AV_SCOPE("AppPointerCheck::invariants_hold");
    for (uint32_t index = 0u; index < bytes; index++)
        if (pattern_[index] != pattern_byte(index)) return false;
    return true;
}

}  // namespace av
