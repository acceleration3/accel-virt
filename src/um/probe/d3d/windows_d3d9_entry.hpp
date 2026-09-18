/* D3d9Entry over the d3d9.dll beside the probe (feature 047, T028), which in
 * the guest is DXVK's: one device on the probe's window, its back buffer
 * cleared, and a system-memory surface to read that back through. Translation
 * only. */
#ifndef AV_WINDOWS_D3D9_ENTRY_HPP
#define AV_WINDOWS_D3D9_ENTRY_HPP

#include "guest/suite/d3d9_entry.hpp"

#include <d3d9.h>

namespace av {

class WindowsD3d9Entry final : public D3d9Entry {
public:
    WindowsD3d9Entry();
    ~WindowsD3d9Entry();
    WindowsD3d9Entry(const WindowsD3d9Entry &) = delete;
    WindowsD3d9Entry &operator=(const WindowsD3d9Entry &) = delete;

    int32_t create_device(void *window, uint32_t width, uint32_t height) override;
    int32_t adapter_ids(uint32_t &vendor, uint32_t &device) override;
    int32_t clear(const uint8_t *colour) override;
    int32_t read_back(uint8_t *pixels, uint32_t bytes) override;
    int32_t present() override;
    void    release() override;

private:
    IDirect3D9       *d3d9_;
    IDirect3DDevice9 *device_;
    IDirect3DSurface9 *readback_;
    uint32_t          width_;
    uint32_t          height_;
};

}  // namespace av

#endif  // AV_WINDOWS_D3D9_ENTRY_HPP
