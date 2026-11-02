#include "device/memory/run_swap_gate.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "device/present/present_engine.hpp"

namespace av {

RunSwapGate::RunSwapGate(BlockSource &blocks) : engines_(blocks, Pool::paged)
{
    AV_SCOPE("RunSwapGate::RunSwapGate");
}

Status RunSwapGate::add(PresentEngine &engine)
{
    AV_SCOPE("RunSwapGate::add");
    for (PresentEngine *const known : engines_)
        if (known == &engine) return Status::ok;
    return engines_.append(&engine);
}

void RunSwapGate::remove(PresentEngine &engine)
{
    AV_SCOPE("RunSwapGate::remove");
    for (uint32_t index = 0u; index < engines_.size(); index++) {
        if (engines_.at(index) != &engine) continue;
        engines_.remove_swap(index);
        return;
    }
}

bool RunSwapGate::begin(Host &host)
{
    AV_SCOPE("RunSwapGate::begin");
    host.require_main_loop("RunSwapGate::begin");
    for (PresentEngine *const engine : engines_) engine->hold_moves();
    return host.pause_guest();
}

void RunSwapGate::end(Host &host, bool guest_held)
{
    AV_SCOPE("RunSwapGate::end");
    if (guest_held) host.resume_guest();
    for (PresentEngine *const engine : engines_) engine->resume_moves();
}

uint32_t RunSwapGate::engines() const
{
    AV_SCOPE("RunSwapGate::engines");
    return engines_.size();
}

}  // namespace av
