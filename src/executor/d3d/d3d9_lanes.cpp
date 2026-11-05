#include "executor/d3d/d3d9_lanes.hpp"

#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/eventfd_wake.hpp"

namespace av {

D3d9Lanes::D3d9Lanes(BlockSource &blocks, EventfdWake &wake, DriverLog &lines)
    : lanes_(blocks), wake_(wake), lines_(lines), marked_(0u), marking_failed_(false)
{
    AV_SCOPE("D3d9Lanes::D3d9Lanes");
}

D3d9Lane *D3d9Lanes::find(const IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9Lanes::find");
    return lanes_.find_if([device](const D3d9Lane &held) { return held.is(device); });
}

D3d9Lane *D3d9Lanes::find(const SliceImporter *importer)
{
    AV_SCOPE("D3d9Lanes::find");
    return lanes_.find_if([importer](const D3d9Lane &held) { return held.is(importer); });
}

D3d9Lane *D3d9Lanes::lane(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9Lanes::lane");
    D3d9Lane *held = find(device);
    if (held != nullptr) return held;
    const Status created = lanes_.create(held, device, *this);
    if (created == Status::ok) return held;
    LogLine(lines_).text("a Direct3D 9 device's lane kept nowhere, status ").number(static_cast<uint32_t>(created)).write();
    return nullptr;
}

void D3d9Lanes::touch(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9Lanes::touch");
    D3d9Lane *const held = find(device);
    if (held != nullptr && held->allowed()) held->touch();
}

void D3d9Lanes::mark_touched()
{
    AV_SCOPE("D3d9Lanes::mark_touched");
    const int wake = wake_.bound() ? wake_.descriptor() : -1;
    while (D3d9Lane *const due = lanes_.find_if([](const D3d9Lane &held) { return held.touched(); })) {
        if (due->mark(marked_ + 1u, wake)) {
            marked_++;
            continue;
        }
        if (!marking_failed_) lines_.write("a Direct3D 9 device's batches could not be marked against the GPU: its buffers are copied");
        marking_failed_ = true;
    }
}

void D3d9Lanes::forget(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9Lanes::forget");
    D3d9Lane *const held = find(device);
    if (held != nullptr) static_cast<void>(lanes_.destroy(held));
}

void D3d9Lanes::forget_all()
{
    AV_SCOPE("D3d9Lanes::forget_all");
    lanes_.destroy_all();
}

bool D3d9Lanes::marking_failed() const
{
    AV_SCOPE("D3d9Lanes::marking_failed");
    return marking_failed_;
}

uint64_t D3d9Lanes::next() const
{
    AV_SCOPE("D3d9Lanes::next");
    return marked_ + 1u;
}

uint64_t D3d9Lanes::marked() const
{
    AV_SCOPE("D3d9Lanes::marked");
    return marked_;
}

uint64_t D3d9Lanes::reached() const
{
    AV_SCOPE("D3d9Lanes::reached");
    uint64_t reached = marked_;
    lanes_.for_each([&reached](const D3d9Lane &held) {
        const uint64_t own = held.reached();
        if (own < reached) reached = own;
    });
    return reached;
}

bool D3d9Lanes::invariants_hold() const
{
    AV_SCOPE("D3d9Lanes::invariants_hold");
    bool sound = lanes_.invariants_hold();
    lanes_.for_each([&sound](const D3d9Lane &held) { sound = sound && held.invariants_hold(); });
    return sound;
}

}
