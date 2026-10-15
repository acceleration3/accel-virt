/* D3d11DeferredEntry over the system's d3d11.dll, which in the guest is
 * Microsoft's runtime on this driver: one device and swap chain on the
 * probe's window, the dynamic buffers, two deferred contexts and the list
 * each finished last. Translation only - what the probe decides is in
 * src/guest/probe/deferred_map_work.cpp. */
#ifndef AV_WINDOWS_D3D11_DEFERRED_ENTRY_HPP
#define AV_WINDOWS_D3D11_DEFERRED_ENTRY_HPP

#include "guest/probe/d3d11_deferred_entry.hpp"

#include <d3d11.h>

namespace av {

class WindowsD3d11DeferredEntry final : public D3d11DeferredEntry {
public:
    WindowsD3d11DeferredEntry();
    ~WindowsD3d11DeferredEntry();
    WindowsD3d11DeferredEntry(const WindowsD3d11DeferredEntry &) = delete;
    WindowsD3d11DeferredEntry &operator=(const WindowsD3d11DeferredEntry &) = delete;

    int32_t  create_device(void *window, uint32_t width, uint32_t height) override;
    int32_t  create_buffers(const uint32_t *sizes, uint32_t count, uint32_t geometry_bytes,
                            uint32_t readback_bytes) override;
    int32_t  create_contexts(uint32_t count) override;
    int32_t  write_constants(uint32_t context, uint32_t buffer, uint8_t value) override;
    int32_t  discard_geometry(uint32_t context, uint32_t bytes, uint8_t value) override;
    int32_t  append_geometry(uint32_t context, uint32_t offset, uint32_t bytes, uint8_t value) override;
    int32_t  finish(uint32_t context) override;
    int32_t  execute(uint32_t context) override;
    int32_t  present() override;
    int32_t  read_geometry(uint8_t *bytes, uint32_t count) override;
    uint64_t committed_bytes() override;
    void     release() override;

private:
    static constexpr uint32_t most_constants = 8u;
    static constexpr uint32_t most_contexts = 4u;

    /* `buffer` mapped on `context` as `type`, `bytes` at `offset` written
     * `value`, and unmapped. */
    int32_t write(uint32_t context, ID3D11Buffer *buffer, D3D11_MAP type, uint32_t offset, uint32_t bytes,
                  uint8_t value);
    /* A dynamic buffer of `bytes` the CPU writes, bound as `bind`. */
    int32_t dynamic_buffer(uint32_t bytes, uint32_t bind, ID3D11Buffer **made);

    IDXGISwapChain      *swapchain_;
    ID3D11Device        *device_;
    ID3D11DeviceContext *immediate_;
    ID3D11Buffer        *constants_[most_constants];
    uint32_t             constant_count_;
    ID3D11Buffer        *geometry_;
    ID3D11Buffer        *readback_;
    ID3D11DeviceContext *deferred_[most_contexts];
    ID3D11CommandList   *lists_[most_contexts];
    uint32_t             context_count_;
};

}  // namespace av

#endif  // AV_WINDOWS_D3D11_DEFERRED_ENTRY_HPP
