/* D3d9ExEntry over the system's d3d9.dll, as WPF's renderer calls it: a
 * device on a 1x1 swap chain, the window's own COPY swap chain beside it, a
 * frame cleared into its buffer and presented once with a dirty region.
 * Translation only. */
#ifndef AV_WINDOWS_D3D9EX_ENTRY_HPP
#define AV_WINDOWS_D3D9EX_ENTRY_HPP

#include "guest/probe/d3d9ex_entry.hpp"

#include <d3d9.h>

namespace av {

class WindowsD3d9ExEntry final : public D3d9ExEntry {
public:
    WindowsD3d9ExEntry();
    ~WindowsD3d9ExEntry();
    WindowsD3d9ExEntry(const WindowsD3d9ExEntry &) = delete;
    WindowsD3d9ExEntry &operator=(const WindowsD3d9ExEntry &) = delete;

    int32_t create_device(void *window) override;
    int32_t create_swap_chain(void *window, uint32_t width, uint32_t height) override;
    int32_t draw(const uint8_t *colour) override;
    int32_t present(uint32_t width, uint32_t height) override;
    int32_t wait_for_blank() override;
    void    release() override;

private:
    IDirect3D9Ex        *d3d9_;
    IDirect3DDevice9Ex  *device_;
    IDirect3DSwapChain9 *chain_;
};

}  // namespace av

#endif  // AV_WINDOWS_D3D9EX_ENTRY_HPP
