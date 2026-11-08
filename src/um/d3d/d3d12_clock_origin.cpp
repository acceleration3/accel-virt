#include "um/d3d/d3d12_clock_origin.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "um/d3d/d3d_session.hpp"
#include "proto/escape/clock_origin_record.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "core/log/keyed_log.hpp"

namespace av::d3d12 {

D3d12ClockOrigin::D3d12ClockOrigin(d3d::D3dSession &session, KeyedLog &lines)
    : session_(session), lines_(lines), clock_(), schedule_()
{
    AV_SCOPE("D3d12ClockOrigin::D3d12ClockOrigin");
}

void D3d12ClockOrigin::refresh(d3d::D3dDeviceChannel &wire, uint32_t queue)
{
    AV_SCOPE("D3d12ClockOrigin::refresh");
    const uint64_t now = clock_.now_nanoseconds();
    if (!schedule_.due(now)) return;
    schedule_.taken(now);
    uint64_t ticks = 0u, frequency = 0u;
    const uint64_t before = clock_.ticks();
    const bool     answered = d3d12proto::av_d3d12_GetClockOrigin12(wire, queue, &ticks, &frequency);
    const uint64_t after = clock_.ticks();
    if (!answered || frequency == 0u) {
        lines_.write("the host answered no GPU clock to calibrate timestamps from");
        return;
    }
    uint8_t bytes[ClockOriginRecord::encoded_size] = {};
    const ClockOriginRecord origin(before + (after - before) / 2u, ticks, frequency);
    if (origin.encode(bytes, sizeof bytes) != Status::ok || session_.escape(bytes, sizeof bytes) != Status::ok)
        lines_.write("the miniport took no GPU clock origin to calibrate timestamps from");
}

}
