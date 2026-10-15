#include "guest/probe/deferred_map_work.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/probe/d3d11_deferred_entry.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

namespace {

/* A prime below 256: a piece's value cycles through every nonzero byte
 * before it repeats. */
constexpr uint32_t value_cycle = 251u;
constexpr uint32_t kibibyte_shift = 10u;

}  // namespace

DeferredMapWork::DeferredMapWork(D3d11DeferredEntry &d3d11)
    : d3d11_(d3d11), made_(false), drawn_(0u), growth_(0u), head_{}
{
    AV_SCOPE("DeferredMapWork::DeferredMapWork");
}

DeferredMapWork::~DeferredMapWork()
{
    AV_SCOPE("DeferredMapWork::~DeferredMapWork");
    if (made_) d3d11_.release();
}

void DeferredMapWork::run(ScenarioSteps &steps, void *window)
{
    AV_SCOPE("DeferredMapWork::run");
    if (steps.check_result("deferred_device", d3d11_.create_device(window, extent, extent)) != Status::ok) return;
    made_ = true;
    if (steps.check_result("deferred_buffers", d3d11_.create_buffers(constant_sizes, constant_count, geometry_bytes,
                                                                     head_bytes)) != Status::ok)
        return;
    if (steps.check_result("deferred_contexts", d3d11_.create_contexts(contexts)) != Status::ok) return;
    if (steps.check_result("deferred_frames", frames()) != Status::ok) return;
    if (steps.check_result("deferred_readback", d3d11_.read_geometry(head_, head_bytes)) != Status::ok) return;
    const uint32_t right = matching();
    steps.check_measured("deferred_contents", right == head_bytes ? Status::ok : Status::invalid_state, right);
    steps.check_measured("deferred_commit", growth_ <= most_growth ? Status::ok : Status::invalid_state,
                         static_cast<uint32_t>(growth_ >> kibibyte_shift));
}

/* The memory is read once the warm frames are drawn and again after the
 * last: growth is what the measured frames kept. Memory given back between
 * the two is no growth. */
int32_t DeferredMapWork::frames()
{
    AV_SCOPE("DeferredMapWork::frames");
    uint64_t before = 0u;
    for (drawn_ = 0u; drawn_ < warm_frames + measured_frames; drawn_++) {
        if (drawn_ == warm_frames) before = d3d11_.committed_bytes();
        const int32_t drawn = frame(drawn_);
        if (drawn < 0) return drawn;
    }
    const uint64_t after = d3d11_.committed_bytes();
    growth_ = after > before ? after - before : 0u;
    return 0;
}

int32_t DeferredMapWork::frame(uint32_t number)
{
    AV_SCOPE("DeferredMapWork::frame");
    const uint32_t context = number % contexts;
    int32_t        result = 0;
    for (uint32_t buffer = 0u; buffer < constant_count && result >= 0; buffer++)
        result = d3d11_.write_constants(context, buffer, value(number, buffer));
    if (result >= 0) result = d3d11_.discard_geometry(context, piece_bytes, value(number, 0u));
    for (uint32_t piece = 1u; piece < pieces && result >= 0; piece++)
        result = d3d11_.append_geometry(context, piece * piece_bytes, piece_bytes, value(number, piece));
    if (result >= 0) result = d3d11_.finish(context);
    if (result >= 0) result = d3d11_.execute(context);
    if (result >= 0) result = d3d11_.present();
    return result;
}

uint8_t DeferredMapWork::value(uint32_t number, uint32_t piece) const
{
    AV_SCOPE("DeferredMapWork::value");
    return static_cast<uint8_t>(1u + (number * pieces + piece) % value_cycle);
}

uint32_t DeferredMapWork::matching() const
{
    AV_SCOPE("DeferredMapWork::matching");
    if (drawn_ == 0u) return 0u;
    uint32_t right = 0u;
    for (uint32_t byte = 0u; byte < head_bytes; byte++)
        right += head_[byte] == value(drawn_ - 1u, byte / piece_bytes) ? 1u : 0u;
    return right;
}

bool DeferredMapWork::invariants_hold() const
{
    AV_SCOPE("DeferredMapWork::invariants_hold");
    if (!made_ && (drawn_ != 0u || growth_ != 0u)) return false;
    return drawn_ <= warm_frames + measured_frames && matching() <= head_bytes;
}

}  // namespace av
