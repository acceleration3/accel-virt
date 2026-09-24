#include "harness/icd/recording_host_driver.hpp"

#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_create_device_record.hpp"
#include "icd/d3d/umd_adapter.hpp"
#include "icd/d3d/umd_device.hpp"

namespace av {

RecordingHostDriver::RecordingHostDriver()
    : build_answer_(d3d::hr_ok), caps_answer_(d3d::hr_ok), fill_answer_(d3d::hr_ok), builds12_(0u), builds11_(0u),
      kmt_adapter_(0u), built_block_(nullptr), caps_runtime_(d3d::RuntimeKind::d3d10_11), caps_type_(0u),
      table_type_(~0u), table_bytes_(0u), tear_downs_(0u), torn_block_(nullptr),
      torn_runtime_(d3d::RuntimeKind::d3d10_11)
{
}

d3d::Hresult RecordingHostDriver::build(d3d::UmdDevice &, d3d::D3d10ddiargCreateDeviceRecord &, std::uint32_t kmt_adapter)
{
    builds11_++;
    kmt_adapter_ = kmt_adapter;
    return build_answer_;
}

d3d::Hresult RecordingHostDriver::build12(d3d::UmdDevice &, d3d::D3d12ddiargCreateDeviceRecord &create,
                                          std::uint32_t kmt_adapter)
{
    builds12_++;
    kmt_adapter_ = kmt_adapter;
    built_block_ = create.hDrvDevice;
    return build_answer_;
}

d3d::Hresult RecordingHostDriver::caps(d3d::UmdAdapter &adapter, d3d::D3d102ddiargGetCapsRecord &args,
                                       std::uint32_t kmt_adapter)
{
    caps_runtime_ = adapter.runtime();
    caps_type_ = args.Type;
    kmt_adapter_ = kmt_adapter;
    return caps_answer_;
}

d3d::Hresult RecordingHostDriver::fill_table12(std::uint32_t table_type, void *, std::size_t bytes)
{
    table_type_ = table_type;
    table_bytes_ = bytes;
    return fill_answer_;
}

void RecordingHostDriver::tear_down(d3d::UmdDevice &device, void *block)
{
    tear_downs_++;
    torn_block_ = block;
    torn_runtime_ = device.adapter().runtime();
}

void RecordingHostDriver::answer(d3d::Hresult build, d3d::Hresult caps, d3d::Hresult fill)
{
    build_answer_ = build;
    caps_answer_ = caps;
    fill_answer_ = fill;
}

std::uint32_t    RecordingHostDriver::builds12() const { return builds12_; }
std::uint32_t    RecordingHostDriver::builds11() const { return builds11_; }
std::uint32_t    RecordingHostDriver::kmt_adapter() const { return kmt_adapter_; }
void            *RecordingHostDriver::built_block() const { return built_block_; }
d3d::RuntimeKind RecordingHostDriver::caps_runtime() const { return caps_runtime_; }
std::uint32_t    RecordingHostDriver::caps_type() const { return caps_type_; }
std::uint32_t    RecordingHostDriver::table_type() const { return table_type_; }
std::size_t      RecordingHostDriver::table_bytes() const { return table_bytes_; }
std::uint32_t    RecordingHostDriver::tear_downs() const { return tear_downs_; }
void            *RecordingHostDriver::torn_block() const { return torn_block_; }
d3d::RuntimeKind RecordingHostDriver::torn_runtime() const { return torn_runtime_; }

}  // namespace av
