#ifndef AV_MOCK_HOST_APIS_HPP
#define AV_MOCK_HOST_APIS_HPP

#include "executor/host_apis.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/d3d/shared_surfaces.hpp"
#include "core/log/driver_log.hpp"
#include "executor/sharing/host_shares.hpp"
#include "harness/icd/recording_log.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/vram_ledger.hpp"
#include "harness/executor/mock_memory_exporter.hpp"

#include <stdint.h>

namespace av {

class MockHostApis final : public HostApis {
public:
    static constexpr uint32_t max_gpus = 12u;
    static constexpr uint32_t uuid_bytes = 16u;

    MockHostApis();

    void add_gpu(const uint8_t *uuid, uint32_t sub_apis);

    uint32_t questions() const;
    uint32_t ledger_questions() const;
    void             give_d3d11(uint32_t index);
    void             give_gl(const GlDisplay *display);
    D3d11HostDevice &d3d11();
    MockMemoryExporter &exporter();
    HostMemorySource   &source();
    void                withhold_memory_source();

    uint32_t gpu_count() override;
    void     gpu_uuid(uint32_t index, uint8_t *out) override;
    int32_t  render_minor(uint32_t index) override;
    uint32_t sub_apis(uint32_t index) override;
    const GlDisplay    *gl_display(uint32_t index) override;
    const OpenClDevice *opencl_device(uint32_t index) override;
    D3d11HostDevice    *d3d11_device(uint32_t index) override;
    D3d12HostDevice    *d3d12_device(uint32_t index) override;
    D3d9HostDevice     *d3d9_device(uint32_t index) override;
    SharedSurfaces     *surfaces(uint32_t index) override;
    HostShares         *shares(uint32_t index) override;
    HostMemorySource   *memory_source(uint32_t index) override;
    VramLedger         *vram_ledger(uint32_t index) override;
    VramLedger         &ledger();

private:
    struct Gpu {
        uint8_t  uuid[uuid_bytes];
        uint32_t sub_apis;
    };

    Gpu             gpus_[max_gpus];
    uint32_t        count_;
    uint32_t        questions_;
    uint32_t        ledger_questions_;
    RecordingLog    log_;
    DriverLog       lines_;
    D3d11HostDevice d3d11_;
    uint32_t        d3d11_index_;
    const GlDisplay *gl_;
    HostHeap        heap_;
    SharedSurfaces  surfaces_;
    HostShares      shares_;
    MockMemoryExporter exporter_;
    HostMemorySource   source_;
    VramLedger         ledger_;
    bool               memory_withheld_;
};

}

#endif
