/* The host's Direct3D: DXVK's dxgi and d3d11 and vkd3d-proton's d3d12, built
 * natively for Linux from third_party/ and loaded into this process.
 *
 * Loaded rather than linked because they are optional: a host without them
 * serves every other sub-API, and a configuration that never built them still
 * links the executor library. Found in the directory RVK_D3D_PATH names, or
 * by the loader's own search when it names none.
 *
 * DXVK's native build must be told which window system it runs under even
 * when it presents to none - it refuses to make a factory otherwise - so the
 * loader names SDL3, whose driver loads with no display at all, unless the
 * environment already chose one. */
#ifndef AV_D3D_RUNTIME_HPP
#define AV_D3D_RUNTIME_HPP

#include <stdint.h>

struct IDXGIAdapter1;
struct ID3D11Device;
struct ID3D11DeviceContext;

namespace av {

class D3dRuntime {
public:
    static constexpr uint32_t uuid_bytes = 16u;

    D3dRuntime();
    ~D3dRuntime();
    D3dRuntime(const D3dRuntime &) = delete;
    D3dRuntime &operator=(const D3dRuntime &) = delete;

    /* Loads what is there. False when DXVK's d3d11 is not: without it nothing
     * here serves anything. vkd3d-proton missing only takes Direct3D 12. */
    [[nodiscard]] bool open();
    void               close();
    bool               has_d3d11() const;
    bool               has_d3d12() const;

    /* The DXGI adapter whose Vulkan device has this UUID, with a reference the
     * caller owns; null for none. */
    IDXGIAdapter1 *adapter_for(const uint8_t *uuid) const;
    /* D3D11CreateDevice on `adapter` at the highest level it offers up to
     * 12_1; the HRESULT, with the device and its immediate context on S_OK. */
    int32_t create_device11(IDXGIAdapter1 *adapter, ID3D11Device *&device, ID3D11DeviceContext *&context,
                            uint32_t &feature_level) const;
    /* vkd3d-proton's D3D12CreateDevice, on the adapter whose Vulkan device has
     * this UUID; `device` is an ID3D12Device, null on failure. */
    int32_t create_device12(const uint8_t *uuid, void *&device) const;

private:
    using CreateFactory = int32_t (*)(const void *riid, void **factory);
    using CreateDevice11 = int32_t (*)(void *adapter, uint32_t driver_type, void *software, uint32_t flags,
                                       const uint32_t *levels, uint32_t level_count, uint32_t sdk_version,
                                       ID3D11Device **device, uint32_t *level, ID3D11DeviceContext **context);
    using CreateDevice12 = int32_t (*)(void *adapter, uint32_t minimum_level, const void *riid, void **device);

    void *open_library(const char *name) const;

    void *dxgi_;
    void *d3d11_;
    void *d3d12_;
    CreateFactory  create_factory_;
    CreateDevice11 create_device11_;
    CreateDevice12 create_device12_;
};

}  // namespace av

#endif  // AV_D3D_RUNTIME_HPP
