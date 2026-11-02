#include "guest/probe/host_loss_work.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/probe/d3d11_shared_entry.hpp"
#include "guest/probe/host_loss_request.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

namespace {

constexpr int32_t  colour_wrong = static_cast<int32_t>(0x80004005u);
constexpr uint32_t milliseconds_per_second = 1000u;
constexpr uint8_t  tolerance = 1u;

bool near(uint8_t read, uint8_t asked)
{
    AV_SCOPE("near");
    return read + tolerance >= asked && asked + tolerance >= read;
}

}

HostLossWork::HostLossWork(D3d11SharedEntry &before, D3d11SharedEntry &after, HostLossRequest &request, Kmt &kmt)
    : before_(before), after_(after), request_(request), kmt_(kmt), made_before_(false), made_after_(false)
{
    AV_SCOPE("HostLossWork::HostLossWork");
}

HostLossWork::~HostLossWork()
{
    AV_SCOPE("HostLossWork::~HostLossWork");
    if (made_after_) after_.release();
    if (made_before_) before_.release();
}

int32_t HostLossWork::draw(D3d11SharedEntry &entry, const uint8_t *colour)
{
    AV_SCOPE("HostLossWork::draw");
    const int32_t made = entry.create_target(0u, width, height);
    if (made < 0) return made;
    entry.clear(0u, colour);
    uint8_t       texel[4] = {};
    const int32_t read = entry.read_texel(0u, width - 1u, height - 1u, texel);
    if (read < 0) return read;
    const bool right = near(texel[0], colour[2]) && near(texel[1], colour[1]) && near(texel[2], colour[0]);
    return right ? 0 : colour_wrong;
}

uint32_t HostLossWork::elapsed_ms(uint64_t started) const
{
    AV_SCOPE("HostLossWork::elapsed_ms");
    uint64_t       frequency = 0u;
    const uint64_t now = kmt_.performance_counter(frequency);
    if (frequency == 0u) return 0u;
    const uint64_t milliseconds = (now - started) * milliseconds_per_second / frequency;
    return milliseconds > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(milliseconds);
}

Status HostLossWork::await_removal(uint32_t &waited_ms)
{
    AV_SCOPE("HostLossWork::await_removal");
    uint64_t       frequency = 0u;
    const uint64_t started = kmt_.performance_counter(frequency);
    for (;;) {
        before_.clear(0u, before_colour);
        uint8_t texel[4] = {};
        const bool refused = before_.read_texel(0u, 0u, 0u, texel) < 0 || before_.removed_reason() < 0;
        waited_ms = elapsed_ms(started);
        if (refused) return Status::ok;
        if (waited_ms >= removal_ms) return Status::invalid_state;
        kmt_.sleep_briefly();
    }
}

void HostLossWork::run(ScenarioSteps &steps)
{
    AV_SCOPE("HostLossWork::run");
    const int32_t made = before_.create_device();
    made_before_ = made >= 0;
    if (steps.check_result("loss_device", made < 0 ? made : draw(before_, before_colour)) != Status::ok) return;
    if (before_.layered()) {
        steps.note("loss_lose", "the device is a layer's: the host's Direct3D 11 device is not the one it runs on");
        return;
    }
    const Status lost = request_.send();
    if (lost == Status::not_supported) {
        steps.note("loss_lose", "the host keeps its devices: it was started without the device-loss layer");
        return;
    }
    if (steps.check("loss_lose", lost) != Status::ok) return;
    uint32_t waited_ms = 0u;
    if (steps.check_measured("loss_removed", await_removal(waited_ms), waited_ms) != Status::ok) return;
    const int32_t reason = before_.removed_reason();
    static_cast<void>(steps.check_measured("loss_reason", reason < 0 ? Status::ok : Status::invalid_state,
                                           static_cast<uint32_t>(reason)));
    const int32_t remade = after_.create_device();
    made_after_ = remade >= 0;
    if (steps.check_result("loss_recreated", remade) != Status::ok) return;
    static_cast<void>(steps.check_result("loss_texel", draw(after_, after_colour)));
}

bool HostLossWork::invariants_hold() const
{
    AV_SCOPE("HostLossWork::invariants_hold");
    return made_before_ || !made_after_;
}

}
