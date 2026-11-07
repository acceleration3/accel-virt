#ifndef AV_LOADER_HOST_APIS_HPP
#define AV_LOADER_HOST_APIS_HPP

#include "executor/host_apis.hpp"
#include "executor/host_gpu_set.hpp"
#include "executor/shim/compositor_device.hpp"
#include "executor/shim/compositor_memory.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d9_host_device.hpp"
#include "executor/shim/d3d_runtime.hpp"
#include "core/support/object_storage.hpp"
#include "executor/d3d/shared_surfaces.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/opencl_device.hpp"
#include "executor/host_ram_ledger.hpp"
#include "executor/vram_gate.hpp"
#include "executor/vram_ledger.hpp"
#include "proto/context/device_uuid.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class ExecutorLog;

class LoaderHostApis final : public HostApis {
public:
    explicit LoaderHostApis(ExecutorLog &log);
    ~LoaderHostApis();

    void open();
    void close();

    uint32_t gpu_count() override;
    void     gpu_uuid(uint32_t index, uint8_t *out) override;
    uint32_t sub_apis(uint32_t index) override;
    int32_t  render_minor(uint32_t index) override;
    const GlDisplay    *gl_display(uint32_t index) override;
    const OpenClDevice *opencl_device(uint32_t index) override;
    D3d11HostDevice    *d3d11_device(uint32_t index) override;
    D3d12HostDevice    *d3d12_device(uint32_t index) override;
    D3d9HostDevice     *d3d9_device(uint32_t index) override;
    SharedSurfaces     *surfaces(uint32_t index) override;
    HostShares         *shares(uint32_t index) override;
    HostMemorySource   *memory_source(uint32_t index) override;
    VramLedger         *vram_ledger(uint32_t index) override;
    HostRamLedger      *host_ram_ledger() override;

private:
    void enumerate();

    void    *instance_;
    DeviceUuid uuids_[HostGpuSet::max_gpus];
    uint32_t sub_apis_[HostGpuSet::max_gpus];
    int32_t  render_minors_[HostGpuSet::max_gpus];
    OpenClDevice opencl_devices_[HostGpuSet::max_gpus];
    GlDisplay    gl_displays_[HostGpuSet::max_gpus];
    D3dRuntime      d3d_runtime_;
    ObjectStorage<D3d11HostDevice> d3d11_devices_[HostGpuSet::max_gpus];
    CompositorDevice compositors_[HostGpuSet::max_gpus];
    ObjectStorage<CompositorMemory> exporters_[HostGpuSet::max_gpus];
    ObjectStorage<HostMemorySource> sources_[HostGpuSet::max_gpus];
    ObjectStorage<D3d12HostDevice> d3d12_devices_[HostGpuSet::max_gpus];
    D3d9HostDevice  d3d9_devices_[HostGpuSet::max_gpus];
    HostHeap                   heap_;
    ObjectStorage<SharedSurfaces> surfaces_[HostGpuSet::max_gpus];
    ObjectStorage<HostShares>  shares_[HostGpuSet::max_gpus];
    VramLedger  ledgers_[HostGpuSet::max_gpus];
    HostRamLedger host_ram_;
    VramGate    vram_gate_;
    uint32_t count_;
    bool     opened_;
};

}

#endif
