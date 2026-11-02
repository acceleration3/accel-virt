#ifndef AV_MAPPED_VIRTIO_FUNCTION_HPP
#define AV_MAPPED_VIRTIO_FUNCTION_HPP

#include "miniport/driver/dxgkrnl_interface.hpp"
#include "miniport/virtio/bar_mappings.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/virtio_pci_layout.hpp"
#include "harness/virtio/fake_common_config.hpp"
#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
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
    DxgkrnlInterface      &interface();
    const VirtioPciLayout &layout() const;
    FakeVirtqueueDevice    queue_device(std::uint16_t index);

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    BarMappings           mappings_;
    VirtioPciLayout       layout_;
};

}

#endif
