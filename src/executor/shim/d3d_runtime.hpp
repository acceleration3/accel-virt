/* The host's Direct3D: DXVK's dxgi, d3d11 and d3d9 and vkd3d-proton's d3d12, built
 * natively for Linux from third_party/ and loaded into this process.
 *
 * Loaded rather than linked because they are optional: a host without them
 * serves every other sub-API, and a configuration that never built them still
 * links the executor library. Found in the directory AV_D3D_PATH names, or
 * by the loader's own search when it names none.
 *
 * DXVK's native build must be told which window system it runs under even
 * when it presents to none - it refuses to make a factory otherwise - so the
 * loader names SDL3, whose driver loads with no display at all, unless the
 * environment already chose one. */
#ifndef AV_D3D_RUNTIME_HPP
#define AV_D3D_RUNTIME_HPP

#include <stdint.h>

struct IDirect3D9;
struct IDXGIAdapter1;
struct ID3D11Device;
struct ID3D11DeviceContext;

namespace av {

class Vkd3dImageExport;

class D3dRuntime {
public:

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
    bool               has_d3d9() const;

    /* The DXGI adapter whose Vulkan device has this UUID, with a reference the
     * caller owns; null for none. */
    IDXGIAdapter1 *adapter_for(const uint8_t *uuid) const;
    /* D3D11CreateDevice on `adapter` at the highest level it offers up to
     * 12_1; the HRESULT, with the device and its immediate context on S_OK. */
    int32_t create_device11(IDXGIAdapter1 *adapter, ID3D11Device *&device, ID3D11DeviceContext *&context,
                            uint32_t &feature_level) const;
    /* DXVK's Direct3D 9, with a reference the caller owns; null without it. */
    IDirect3D9 *create_d3d9() const;
    /* A vkd3d-proton device of its own on the Vulkan device with this UUID;
     * `device` is an ID3D12Device, null on failure. */
    int32_t create_device12(const uint8_t *uuid, void *&device) const;
    /* Whether this vkd3d-proton imports host memory as a heap: the patched
     * one does (tools/vkd3d-proton-patches), a stock one does not. */
    bool    imports_host_heaps() const;
    /* `size` bytes of host memory at `address` as an ID3D12Heap of `device`'s
     * that only buffers can be placed in; the HRESULT, `heap` null on failure. */
    int32_t open_host_heap(void *device, void *address, uint64_t size, void *&heap) const;
    /* The same pages as the dma-buf `fd`, which the caller keeps. */
    int32_t open_dmabuf_heap(void *device, void *address, int fd, uint64_t size, void *&heap) const;
    /* A shared committed texture of vkd3d-proton's - `resource` an
     * ID3D12Resource - as the fd of its memory and the image it was made as;
     * the HRESULT. */
    int32_t export_image(void *resource, Vkd3dImageExport &image) const;
    /* A shared fence's timeline semaphore - `fence` an ID3D12Fence - as an fd
     * the caller owns; the HRESULT, `fd` -1 on failure. */
    int32_t export_fence(void *fence, int &fd) const;
    /* D3D12SerializeVersionedRootSignature on a D3D12_VERSIONED_ROOT_SIGNATURE_DESC;
     * `blob` an ID3DBlob the caller owns, null on failure. */
    int32_t serialize_root_signature(const void *desc, void *&blob) const;

private:
    using CreateFactory = int32_t (*)(const void *riid, void **factory);
    using CreateDevice11 = int32_t (*)(void *adapter, uint32_t driver_type, void *software, uint32_t flags,
                                       const uint32_t *levels, uint32_t level_count, uint32_t sdk_version,
                                       ID3D11Device **device, uint32_t *level, ID3D11DeviceContext **context);
    using GetInterface12 = int32_t (*)(const void *clsid, const void *riid, void **object);
    using Create9 = IDirect3D9 *(*)(uint32_t sdk_version);
    using OpenHostHeap = int32_t (*)(void *device, void *address, uint64_t size, const void *riid, void **heap);
    using OpenDmabufHeap = int32_t (*)(void *device, void *address, int fd, uint64_t size, const void *riid,
                                       void **heap);
    using SerializeRoot = int32_t (*)(const void *desc, void **blob, void **errors);
    using ExportImage = int32_t (*)(void *resource, Vkd3dImageExport *image);
    using ExportFence = int32_t (*)(void *fence, int *fd);

    void *open_library(const char *name) const;
    /* Where the Vulkan device with this UUID comes in the loader's
     * enumeration, which is what vkd3d-proton selects a device by; ~0u when
     * none has it. */
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
    OpenHostHeap   open_host_heap_;
    OpenDmabufHeap open_dmabuf_heap_;
    SerializeRoot  serialize_root_;
    ExportImage    export_image_;
    ExportFence    export_fence_;
};

}  // namespace av

#endif  // AV_D3D_RUNTIME_HPP
