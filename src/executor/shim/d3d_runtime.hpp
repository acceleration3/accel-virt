#ifndef AV_D3D_RUNTIME_HPP
#define AV_D3D_RUNTIME_HPP

#include <stdint.h>

struct IDirect3D9;
struct IDXGIAdapter1;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct AvVramGate;

namespace av {

class Vkd3dImageExport;

class D3dRuntime {
public:

    D3dRuntime();
    ~D3dRuntime();
    D3dRuntime(const D3dRuntime &) = delete;
    D3dRuntime &operator=(const D3dRuntime &) = delete;

    [[nodiscard]] bool open();
    void               close();

    void give_vram_gate(const AvVramGate &gate) const;

    IDXGIAdapter1 *adapter_for(const uint8_t *uuid) const;
    int32_t create_device11(IDXGIAdapter1 *adapter, ID3D11Device *&device, ID3D11DeviceContext *&context,
                            uint32_t &feature_level) const;
    IDirect3D9 *create_d3d9() const;
    int32_t create_device12(const uint8_t *uuid, void *&device) const;
    int32_t open_dmabuf_heap(void *device, void *address, int fd, uint64_t size, void *&heap) const;
    int32_t export_image(void *resource, Vkd3dImageExport &image) const;
    int32_t export_fence(void *fence, int &fd) const;
    int32_t serialize_root_signature(const void *desc, void *&blob) const;

private:
    using CreateFactory = int32_t (*)(const void *riid, void **factory);
    using CreateDevice11 = int32_t (*)(void *adapter, uint32_t driver_type, void *software, uint32_t flags,
                                       const uint32_t *levels, uint32_t level_count, uint32_t sdk_version,
                                       ID3D11Device **device, uint32_t *level, ID3D11DeviceContext **context);
    using GetInterface12 = int32_t (*)(const void *clsid, const void *riid, void **object);
    using Create9 = IDirect3D9 *(*)(uint32_t sdk_version);
    using OpenDmabufHeap = int32_t (*)(void *device, void *address, int fd, uint64_t size, const void *riid,
                                       void **heap);
    using SerializeRoot = int32_t (*)(const void *desc, void **blob, void **errors);
    using ExportImage = int32_t (*)(void *resource, Vkd3dImageExport *image);
    using ExportFence = int32_t (*)(void *fence, int *fd);
    using SetVramGate = void (*)(const AvVramGate *gate);

    void *open_library(const char *name) const;
    uint32_t vulkan_index(const uint8_t *uuid) const;

    void *dxgi_;
    void *d3d11_;
    void *d3d12core_;
    void *d3d12_;
    void *d3d9_;
    CreateFactory  create_factory_;
    CreateDevice11 create_device11_;
    GetInterface12 get_interface12_;
    Create9        create9_;
    OpenDmabufHeap open_dmabuf_heap_;
    SerializeRoot  serialize_root_;
    ExportImage    export_image_;
    ExportFence    export_fence_;
};

}

#endif
