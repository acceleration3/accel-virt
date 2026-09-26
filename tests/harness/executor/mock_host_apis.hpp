/* A host whose GPUs are whatever a test says they are: none, two, or one whose
 * OpenCL can run but cannot import. */
#ifndef AV_MOCK_HOST_APIS_HPP
#define AV_MOCK_HOST_APIS_HPP

#include "device/executor/host_apis.hpp"
#include "device/shim/d3d11_host_device.hpp"

#include <stdint.h>

namespace av {

class MockHostApis final : public HostApis {
public:
    /* More than HostGpuSet holds, so that a test can drive the ceiling. */
    static constexpr uint32_t max_gpus = 12u;
    static constexpr uint32_t uuid_bytes = 16u;

    MockHostApis();

    /* Appends a GPU. `kinds` is indexed by the same order as the ABI's bits:
     * Vulkan, OpenGL, OpenCL. */
    void add_gpu(const uint8_t *uuid, uint32_t sub_apis, uint32_t vulkan_kinds,
                 uint32_t opengl_kinds, uint32_t opencl_kinds);

    /* How often each query was asked, so a test can prove the set asks once. */
    uint32_t questions() const;
    /* GPU `index` has a Direct3D 11 device - never opened, so only its shared
     * table does anything - which d3d11_device answers for it alone. */
    void             give_d3d11(uint32_t index);
    D3d11HostDevice &d3d11();

    uint32_t gpu_count() override;
    void     gpu_uuid(uint32_t index, uint8_t *out) override;
    int32_t  render_minor(uint32_t index) override;
    uint32_t sub_apis(uint32_t index) override;
    uint32_t import_kinds(uint32_t index, uint32_t sub_api) override;
    /* Null for every GPU but the one given a Direct3D 11 device: a mock GPU
     * has no display or OpenCL device behind it, so a context of either API on
     * one is refused when it opens. */
    const GlDisplay    *gl_display(uint32_t index) override;
    const OpenClDevice *opencl_device(uint32_t index) override;
    D3d11HostDevice    *d3d11_device(uint32_t index) override;
    D3d12HostDevice    *d3d12_device(uint32_t index) override;

private:
    struct Gpu {
        uint8_t  uuid[uuid_bytes];
        uint32_t sub_apis;
        uint32_t kinds[3];
    };

    Gpu             gpus_[max_gpus];
    uint32_t        count_;
    uint32_t        questions_;
    D3d11HostDevice d3d11_;
    uint32_t        d3d11_index_;
};

}  // namespace av

#endif  // AV_MOCK_HOST_APIS_HPP
