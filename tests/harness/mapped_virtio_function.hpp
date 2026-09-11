/* A fake virtio-gpu function with its BARs mapped the way a started adapter
 * maps them: what the transport and the device classes are built over. */
#ifndef AV_MAPPED_VIRTIO_FUNCTION_HPP
#define AV_MAPPED_VIRTIO_FUNCTION_HPP

#include "../../src/core/bar_mappings.hpp"
#include "../../src/core/dxgkrnl_interface.hpp"
#include "../../src/core/status.hpp"
#include "../../src/core/virtio_pci_layout.hpp"
#include "fake_common_config.hpp"
#include "fake_dxgkrnl_adapter.hpp"
#include "fake_virtio_gpu_function.hpp"
#include "fake_virtqueue_device.hpp"
#include "mock_platform.hpp"

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
