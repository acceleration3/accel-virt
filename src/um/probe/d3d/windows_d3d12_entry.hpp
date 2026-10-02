/* D3d12Entry over the d3d12.dll beside the probe (feature 047, T028), which in
 * the guest is vkd3d-proton's: one device on the first adapter DXGI lists, a
 * direct queue, a render target committed on its own heap, a readback buffer
 * of its rows, and a fence to wait on. Translation only. */
#ifndef AV_WINDOWS_D3D12_ENTRY_HPP
#define AV_WINDOWS_D3D12_ENTRY_HPP

#include "guest/probe/d3d12_entry.hpp"

#include <d3d12.h>
#include <dxgi1_4.h>

namespace av {

class WindowsD3d12Entry final : public D3d12Entry {
public:
    WindowsD3d12Entry();
    ~WindowsD3d12Entry();
    WindowsD3d12Entry(const WindowsD3d12Entry &) = delete;
    WindowsD3d12Entry &operator=(const WindowsD3d12Entry &) = delete;

    int32_t create_device(uint32_t &feature_level) override;
    int32_t adapter_luid(uint64_t &luid) override;
    int32_t create_queue() override;
    int32_t create_render_target(uint32_t width, uint32_t height) override;
    int32_t clear_and_copy(const uint8_t *colour) override;
    int32_t wait() override;
    int32_t read_back(uint8_t *pixels, uint32_t bytes) override;
    void    release() override;

private:
    IDXGIFactory4             *factory_;
    IDXGIAdapter1             *adapter_;
    ID3D12Device              *device_;
    ID3D12CommandQueue        *queue_;
    ID3D12CommandAllocator    *allocator_;
    ID3D12GraphicsCommandList *commands_;
    ID3D12DescriptorHeap      *views_;
    ID3D12Resource            *target_;
    ID3D12Resource            *readback_;
    ID3D12Fence               *fence_;
    void                      *reached_;
    uint64_t                   signalled_;
    uint32_t                   width_;
    uint32_t                   height_;
    /* What a copy out of a texture rounds each row up to. */
    uint32_t                   row_pitch_;
};

}  // namespace av

#endif  // AV_WINDOWS_D3D12_ENTRY_HPP
