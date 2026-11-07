#include "executor/d3d/d3d12_session.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

bool D3d12Session::on_GetClockOrigin12(uint32_t queue, uint64_t &ticks, uint64_t &frequency)
{
    AV_SCOPE("D3d12Session::on_GetClockOrigin12");
    ticks = 0u;
    frequency = 0u;
    ID3D12CommandQueue *const on = objects_.find_as<ID3D12CommandQueue>(queue, D3dObjectKind::queue12);
    if (on == nullptr) return false;
    UINT64 rate = 0u, gpu = 0u, cpu = 0u;
    if (FAILED(on->GetTimestampFrequency(&rate)) || FAILED(on->GetClockCalibration(&gpu, &cpu)) || rate == 0u) return true;
    ticks = gpu;
    frequency = rate;
    return true;
}

}
