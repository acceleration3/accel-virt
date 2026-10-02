#include "guest/probe/d3d9_work.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/probe/bmp_image.hpp"
#include "guest/probe/d3d9_entry.hpp"
#include "guest/probe/image_sink.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

namespace {

constexpr uint8_t tolerance = 1u;

}  // namespace

D3d9Work::D3d9Work(D3d9Entry &d3d9, ImageSink &images)
    : d3d9_(d3d9), images_(images), adapter_(0u), made_(false), read_(false), frame_{}
{
    AV_SCOPE("D3d9Work::D3d9Work");
}

D3d9Work::~D3d9Work()
{
    AV_SCOPE("D3d9Work::~D3d9Work");
    if (made_) d3d9_.release();
}

void D3d9Work::run(ScenarioSteps &steps, void *window)
{
    AV_SCOPE("D3d9Work::run");
    if (steps.check_result("device", d3d9_.create_device(window, extent, extent)) != Status::ok) return;
    made_ = true;
    uint32_t vendor = 0u;
    uint32_t device = 0u;
    if (steps.check_result("adapter_ids", d3d9_.adapter_ids(vendor, device)) != Status::ok) return;
    adapter_ = (static_cast<uint64_t>(vendor) << 32) | device;
    if (steps.check_result("clear", d3d9_.clear(colour)) != Status::ok) return;
    if (steps.check_result("readback", d3d9_.read_back(frame_, sizeof frame_)) != Status::ok) return;
    read_ = true;
    /* A locked surface holds BGRA rows from the top. */
    images_.save("d3d9-clear", BmpImage(frame_, extent, extent, BmpImage::Rows::top_first,
                                        BmpImage::Order::blue_first));
    const uint32_t right = coloured();
    steps.check_measured("pixels", right == pixels ? Status::ok : Status::invalid_state, right);
    static_cast<void>(steps.check_result("present", d3d9_.present()));
}

uint64_t D3d9Work::adapter() const
{
    AV_SCOPE("D3d9Work::adapter");
    return adapter_;
}

uint32_t D3d9Work::coloured() const
{
    AV_SCOPE("D3d9Work::coloured");
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

bool D3d9Work::invariants_hold() const
{
    AV_SCOPE("D3d9Work::invariants_hold");
    if (!made_ && (read_ || adapter_ != 0u)) return false;
    return coloured() <= pixels;
}

}  // namespace av
