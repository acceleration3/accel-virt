#include "guest/probe/shared_clear_work.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/probe/d3d11_shared_entry.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

namespace {

/* A driver may round a clear colour either way at the half step. */
constexpr uint8_t tolerance = 1u;

bool near(uint8_t read, uint8_t asked)
{
    AV_SCOPE("near");
    return read + tolerance >= asked && asked + tolerance >= read;
}

}  // namespace

SharedClearWork::SharedClearWork(D3d11SharedEntry &d3d11)
    : d3d11_(d3d11), made_(false), made_targets_(0u), read_(0u), texels_{}
{
    AV_SCOPE("SharedClearWork::SharedClearWork");
}

SharedClearWork::~SharedClearWork()
{
    AV_SCOPE("SharedClearWork::~SharedClearWork");
    if (made_) d3d11_.release();
}

void SharedClearWork::run(ScenarioSteps &steps)
{
    AV_SCOPE("SharedClearWork::run");
    if (steps.check_result("shared_device", d3d11_.create_device()) != Status::ok) return;
    made_ = true;
    if (steps.check_result("shared_targets", make_targets()) != Status::ok) return;
    for (uint32_t target = 0u; target < targets; target++) d3d11_.clear(target, colours[target]);
    if (steps.check_result("shared_readback", read_texels()) != Status::ok) return;
    const uint32_t right = coloured();
    steps.check_measured("shared_texels", right == targets ? Status::ok : Status::invalid_state, right);
    static_cast<void>(steps.check_result("shared_device_kept", d3d11_.removed_reason()));
}

int32_t SharedClearWork::make_targets()
{
    AV_SCOPE("SharedClearWork::make_targets");
    for (; made_targets_ < targets; made_targets_++) {
        const int32_t made = d3d11_.create_target(made_targets_, widths[made_targets_], heights[made_targets_]);
        if (made < 0) return made;
    }
    return 0;
}

/* The last texel: rows placed other than where the texture's reader looks
 * for them move it first. */
int32_t SharedClearWork::read_texels()
{
    AV_SCOPE("SharedClearWork::read_texels");
    for (; read_ < targets; read_++) {
        const int32_t asked = d3d11_.read_texel(read_, widths[read_] - 1u, heights[read_] - 1u, texels_[read_]);
        if (asked < 0) return asked;
    }
    return 0;
}

uint32_t SharedClearWork::coloured() const
{
    AV_SCOPE("SharedClearWork::coloured");
    uint32_t right = 0u;
    for (uint32_t target = 0u; target < read_; target++) {
        const uint8_t *const texel = texels_[target];
        const uint8_t *const colour = colours[target];
        /* BGRA against the colour's red-first bytes. */
        right += near(texel[0], colour[2]) && near(texel[1], colour[1]) && near(texel[2], colour[0]) ? 1u : 0u;
    }
    return right;
}

bool SharedClearWork::invariants_hold() const
{
    AV_SCOPE("SharedClearWork::invariants_hold");
    if (!made_ && (made_targets_ != 0u || read_ != 0u)) return false;
    return made_targets_ <= targets && read_ <= made_targets_;
}

}  // namespace av
