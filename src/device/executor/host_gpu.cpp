#include "device/executor/host_gpu.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "device/executor/host_apis.hpp"

namespace av {

namespace {

/* The three sub-API bits, in the order their kinds are cached. */
constexpr uint32_t sub_api_bits[] = {AV_EXECUTOR_SUB_API_VULKAN, AV_EXECUTOR_SUB_API_OPENGL,
                                     AV_EXECUTOR_SUB_API_OPENCL, AV_EXECUTOR_SUB_API_D3D};

}  // namespace

HostGpu::HostGpu() : uuid_(), sub_apis_(0u), kinds_{}, index_(0u), known_(false)
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
    for (uint32_t slot = 0u; slot < sub_api_count; slot++) {
        /* A GPU that cannot run an API cannot import for it either, whatever
         * the loader would answer. */
        kinds_[slot] = ((sub_apis_ & sub_api_bits[slot]) != 0u)
                           ? apis.import_kinds(index, sub_api_bits[slot])
                           : 0u;
    }
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

uint32_t HostGpu::import_kinds(uint32_t sub_api) const
{
    AV_SCOPE("HostGpu::import_kinds");
    const uint32_t slot = slot_of(sub_api);
    return slot < sub_api_count ? kinds_[slot] : 0u;
}

uint32_t HostGpu::index() const
{
    AV_SCOPE("HostGpu::index");
    return index_;
}

uint32_t HostGpu::slot_of(uint32_t sub_api) const
{
    AV_SCOPE("HostGpu::slot_of");
    for (uint32_t slot = 0u; slot < sub_api_count; slot++) {
        if (sub_api_bits[slot] == sub_api) return slot;
    }
    return sub_api_count;
}

bool HostGpu::invariants_hold() const
{
    AV_SCOPE("HostGpu::invariants_hold");
    if (!known_) return sub_apis_ == 0u;
    for (uint32_t slot = 0u; slot < sub_api_count; slot++) {
        if (kinds_[slot] != 0u && (sub_apis_ & sub_api_bits[slot]) == 0u) return false;
    }
    return true;
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
