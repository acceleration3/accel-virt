#include "guest/probe/d3d12_work.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/probe/bmp_image.hpp"
#include "guest/probe/d3d12_entry.hpp"
#include "guest/probe/image_sink.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

namespace {

constexpr uint8_t tolerance = 1u;

}  // namespace

D3d12Work::D3d12Work(D3d12Entry &d3d12, ImageSink &images)
    : d3d12_(d3d12), images_(images), adapter_(0u), feature_level_(0u), made_(false), read_(false), frame_{}
{
    AV_SCOPE("D3d12Work::D3d12Work");
}

D3d12Work::~D3d12Work()
{
    AV_SCOPE("D3d12Work::~D3d12Work");
    if (made_) d3d12_.release();
}

void D3d12Work::run(ScenarioSteps &steps)
{
    AV_SCOPE("D3d12Work::run");
    if (steps.check_result("device", d3d12_.create_device(feature_level_)) != Status::ok) return;
    made_ = true;
    steps.check_measured("feature_level", Status::ok, feature_level_);
    if (steps.check_result("adapter_luid", d3d12_.adapter_luid(adapter_)) != Status::ok) return;
    if (steps.check_result("queue", d3d12_.create_queue()) != Status::ok) return;
    if (steps.check_result("target", d3d12_.create_render_target(extent, extent)) != Status::ok) return;
    if (steps.check_result("clear", d3d12_.clear_and_copy(colour)) != Status::ok) return;
    if (steps.check_result("fence", d3d12_.wait()) != Status::ok) return;
    if (steps.check_result("readback", d3d12_.read_back(frame_, sizeof frame_)) != Status::ok) return;
    read_ = true;
    images_.save("d3d12-clear", BmpImage(frame_, extent, extent, BmpImage::Rows::top_first));
    const uint32_t right = coloured();
    steps.check_measured("pixels", right == pixels ? Status::ok : Status::invalid_state, right);
}

uint64_t D3d12Work::adapter() const
{
    AV_SCOPE("D3d12Work::adapter");
    return adapter_;
}

uint32_t D3d12Work::feature_level() const
{
    AV_SCOPE("D3d12Work::feature_level");
    return feature_level_;
}

uint32_t D3d12Work::coloured() const
{
    AV_SCOPE("D3d12Work::coloured");
    uint32_t right = 0u;
    for (uint32_t pixel = 0u; pixel < pixels; pixel++) {
        const uint8_t *const bytes = &frame_[pixel * pixel_bytes];
        bool                 same = true;
        /* RGBA, as the copy out of the render target leaves it. */
        for (uint32_t channel = 0u; channel < 3u; channel++)
            same = same && bytes[channel] + tolerance >= colour[channel] &&
                   colour[channel] + tolerance >= bytes[channel];
        right += same ? 1u : 0u;
    }
    return right;
}

bool D3d12Work::invariants_hold() const
{
    AV_SCOPE("D3d12Work::invariants_hold");
    if (!made_ && (read_ || adapter_ != 0u || feature_level_ != 0u)) return false;
    return coloured() <= pixels;
}

}  // namespace av
