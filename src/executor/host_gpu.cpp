#include "executor/host_gpu.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "executor/host_apis.hpp"

namespace av {

HostGpu::HostGpu() : uuid_(), sub_apis_(0u), index_(0u), known_(false)
{
    AV_SCOPE("HostGpu::HostGpu");
}

void HostGpu::describe(HostApis &apis, uint32_t index)
{
    AV_SCOPE("HostGpu::describe");
    uint8_t reported[DeviceUuid::bytes] = {};
    apis.gpu_uuid(index, reported);
    uuid_ = DeviceUuid(reported);
    sub_apis_ = apis.sub_apis(index);
    index_ = index;
    known_ = true;
}

bool HostGpu::matches(const uint8_t *uuid) const
{
    AV_SCOPE("HostGpu::matches");
    return known_ && uuid_.equals(uuid);
}

uint32_t HostGpu::sub_apis() const
{
    AV_SCOPE("HostGpu::sub_apis");
    return sub_apis_;
}

uint32_t HostGpu::index() const
{
    AV_SCOPE("HostGpu::index");
    return index_;
}

bool HostGpu::invariants_hold() const
{
    AV_SCOPE("HostGpu::invariants_hold");
    return known_ || sub_apis_ == 0u;
}

void HostGpu::report(StateWriter &writer) const
{
    AV_SCOPE("HostGpu::report");
    char       text[DeviceUuid::bytes * 2u + 1u];
    TextBuffer uuid(text, sizeof text);
    uuid_.append_hex(uuid);
    writer.open("gpu", index_);
    writer.text("uuid", uuid.text());
    writer.hex("sub_apis", sub_apis_);
    writer.close();
}

}  // namespace av
