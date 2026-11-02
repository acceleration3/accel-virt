#include "guest/probe/d3d11_work.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/probe/bmp_image.hpp"
#include "guest/probe/d3d11_entry.hpp"
#include "guest/probe/image_sink.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

namespace {
constexpr uint8_t tolerance = 1u;

}

D3d11Work::D3d11Work(D3d11Entry &d3d11, ImageSink &images)
    : d3d11_(d3d11), images_(images), adapter_(0u), feature_level_(0u), made_(false), read_(false), frame_{}
{
    AV_SCOPE("D3d11Work::D3d11Work");
}

D3d11Work::~D3d11Work()
{
    AV_SCOPE("D3d11Work::~D3d11Work");
    if (made_) d3d11_.release();
}

void D3d11Work::run(ScenarioSteps &steps, void *window)
{
    AV_SCOPE("D3d11Work::run");
    if (steps.check_result("device", d3d11_.create_device(window, extent, extent, feature_level_)) != Status::ok)
        return;
    made_ = true;
    steps.check_measured("feature_level", Status::ok, feature_level_);
    if (steps.check_result("adapter_luid", d3d11_.adapter_luid(adapter_)) != Status::ok) return;
    if (steps.check_result("target", d3d11_.create_render_target()) != Status::ok) return;
    d3d11_.clear(colour);
    if (steps.check_result("readback", d3d11_.read_back(frame_, sizeof frame_)) != Status::ok) return;
    read_ = true;
    images_.save("d3d11-clear", BmpImage(frame_, extent, extent, BmpImage::Rows::top_first,
                                         BmpImage::Order::blue_first));
    const uint32_t right = coloured();
    steps.check_measured("pixels", right == pixels ? Status::ok : Status::invalid_state, right);
    static_cast<void>(steps.check_result("present", d3d11_.present()));
}

uint64_t D3d11Work::adapter() const
{
    AV_SCOPE("D3d11Work::adapter");
    return adapter_;
}

uint32_t D3d11Work::feature_level() const
{
    AV_SCOPE("D3d11Work::feature_level");
    return feature_level_;
}

uint32_t D3d11Work::coloured() const
{
    AV_SCOPE("D3d11Work::coloured");
    uint32_t right = 0u;
    for (uint32_t pixel = 0u; pixel < pixels; pixel++) {
        const uint8_t *const bytes = &frame_[pixel * pixel_bytes];
        const bool blue = bytes[0] + tolerance >= colour[2] && colour[2] + tolerance >= bytes[0];
        const bool green = bytes[1] + tolerance >= colour[1] && colour[1] + tolerance >= bytes[1];
        const bool red = bytes[2] + tolerance >= colour[0] && colour[0] + tolerance >= bytes[2];
        right += blue && green && red ? 1u : 0u;
    }
    return right;
}

bool D3d11Work::invariants_hold() const
{
    AV_SCOPE("D3d11Work::invariants_hold");
    if (!made_ && (read_ || adapter_ != 0u || feature_level_ != 0u)) return false;
    return coloured() <= pixels;
}

}
