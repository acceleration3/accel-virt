/* A fake virtio-gpu function with its BARs mapped the way a started adapter
 * maps them: what the transport and the device classes are built over. */
#ifndef AV_MAPPED_VIRTIO_FUNCTION_HPP
#define AV_MAPPED_VIRTIO_FUNCTION_HPP

#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/pci/bar_mappings.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_pci_layout.hpp"
#include "harness/device/fake_common_config.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class MappedVirtioFunction {
public:
    MappedVirtioFunction();
    MappedVirtioFunction(const MappedVirtioFunction &) = delete;
    MappedVirtioFunction &operator=(const MappedVirtioFunction &) = delete;

    Status map();

    MockPlatform          &platform();
    FakeVirtioGpuFunction &function();
    FakeCommonConfig      &common();
    BarMappings           &mappings();
    /* dxgkrnl's callbacks as the driver reaches them. */
    DxgkrnlInterface      &interface();
    const VirtioPciLayout &layout() const;
    /* The device side of an activated queue. */
    FakeVirtqueueDevice    queue_device(std::uint16_t index);

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    BarMappings           mappings_;
    VirtioPciLayout       layout_;
};

}  // namespace av

#endif  // AV_MAPPED_VIRTIO_FUNCTION_HPP
