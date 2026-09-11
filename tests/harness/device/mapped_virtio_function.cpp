#include "harness/device/mapped_virtio_function.hpp"

#include "core/ddi/dxgk_device_info_record.hpp"
#include "core/pci/pci_base_addresses.hpp"
#include "core/pci/resource_list.hpp"
#include "core/virtio/virtio_capability_walk.hpp"

namespace av {

MappedVirtioFunction::MappedVirtioFunction()
    : platform_(), function_(), dxgkrnl_(platform_, function_), interface_(platform_), mappings_(platform_, interface_),
      layout_() {}

Status MappedVirtioFunction::map()
{
    ResourceList resources(platform_);
    DxgkDeviceInfoRecord info;
    Status status = interface_.initialize(&dxgkrnl_.record());
    if (status == Status::ok) status = interface_.get_device_information(info);
    if (status == Status::ok) status = resources.initialize(info.TranslatedResourceList, ResourceList::max_bytes);
    if (status == Status::ok) status = VirtioCapabilityWalk(function_.config_space(), 256u).walk(layout_);
    const PciBaseAddresses bars(function_.config_space(), 256u);
    if (status == Status::ok) status = mappings_.map(bars, resources, layout_);
    return status;
}

MockPlatform          &MappedVirtioFunction::platform() { return platform_; }
FakeVirtioGpuFunction &MappedVirtioFunction::function() { return function_; }
FakeCommonConfig      &MappedVirtioFunction::common() { return function_.common(); }
BarMappings           &MappedVirtioFunction::mappings() { return mappings_; }
const VirtioPciLayout &MappedVirtioFunction::layout() const { return layout_; }

FakeVirtqueueDevice MappedVirtioFunction::queue_device(std::uint16_t index)
{
    const FakeCommonConfig::Queue &queue = function_.common().queue(index);
    return FakeVirtqueueDevice(platform_, queue.desc, queue.driver, queue.device, queue.size);
}

}  // namespace av
