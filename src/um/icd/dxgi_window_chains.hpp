/* The windows' DXGI flip chains (icd::WindowChains) over d3d11.dll and
 * dxgi.dll, loaded the first time a chain is asked for, so a process that
 * never presents through one never loads them.
 *
 * One Direct3D 11 device per adapter, on rvkumd as any application's is, and
 * per window a flip chain of two R8G8B8A8 buffers and max_slots shared
 * textures. A slot's object id is the key the miniport minted into its
 * allocation, read through D3DKMT from the NT handle the texture is shared by
 * - the AllocationDescriptor and SharedObjectRecord every kernel surface
 * carries - which is how OpenGL and Vulkan name any shared object.
 *
 * Presenting draws the slot into the chain's back buffer upside down - a slot
 * holds its frame bottom row first, as OpenGL stores it - with two shaders
 * compiled at the device's making through d3dcompiler_47.dll, then presents it
 * and waits for the draw, so the slot may be written again. The
 * chains and their devices are used under `lock`: an application's thread
 * opens a window's chain while the present thread presents another's. */
#ifndef AV_UM_DXGI_WINDOW_CHAINS_HPP
#define AV_UM_DXGI_WINDOW_CHAINS_HPP

#include "icd/common/window_chains.hpp"
#include "um/icd/com_reference.hpp"

#include <stdint.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11PixelShader;
struct ID3D11Query;
struct ID3D11RasterizerState;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;
struct ID3D11VertexShader;
struct IDXGIFactory2;
struct IDXGISwapChain1;

namespace av {

class Kmt;

namespace icd {
class LogSink;
class ThreadLock;
}  // namespace icd

class DxgiWindowChains final : public icd::WindowChains {
public:
    static constexpr uint32_t max_windows = 16u;
    static constexpr uint32_t max_devices = 4u;
    /* Looks for a finished copy this often, this many times: what a GPU that
     * finishes nothing in two seconds is given before the slot is let go. */
    static constexpr uint32_t copy_looks = 2000u;

    DxgiWindowChains(Kmt &kmt, icd::ThreadLock &lock, icd::LogSink &log);
    ~DxgiWindowChains();
    DxgiWindowChains(const DxgiWindowChains &) = delete;
    DxgiWindowChains &operator=(const DxgiWindowChains &) = delete;

    bool open(void *window, uint64_t luid, uint32_t width, uint32_t height, uint32_t slots, uint64_t *ids,
              uint64_t *bytes) override;
    bool present(void *window, uint32_t slot) override;
    void close(void *window) override;

private:
    /* One adapter's device, and the kernel device a slot's handle is opened
     * on to read its key. */
    struct Device {
        uint64_t                            luid;
        ComReference<ID3D11Device>          device;
        ComReference<ID3D11DeviceContext>   context;
        ComReference<IDXGIFactory2>         factory;
        ComReference<ID3D11VertexShader>    corners;
        ComReference<ID3D11PixelShader>     flipped;
        ComReference<ID3D11RasterizerState> unculled;
        uint32_t                            kmt_adapter;
        uint32_t                            kmt_device;
    };
    /* One window's chain and slots. */
    struct Chain {
        void                                   *window;
        uint32_t                                device;
        ComReference<IDXGISwapChain1>           chain;
        ComReference<ID3D11Texture2D>           slots[max_slots];
        ComReference<ID3D11ShaderResourceView>  views[max_slots];
        uint64_t                                ids[max_slots];
        uint64_t                                bytes[max_slots];
        ComReference<ID3D11Query>               copied;
        uint32_t                                width;
        uint32_t                                height;
        uint32_t                                count;
    };

    [[nodiscard]] bool loaded();
    /* The device of the adapter `luid` names, made the first time; max_devices
     * for none. */
    uint32_t           device_for(uint64_t luid);
    [[nodiscard]] bool make_device(uint64_t luid, Device &made);
    [[nodiscard]] bool kernel_device(uint64_t luid, Device &made);
    /* The fullscreen triangle and the flipped read of a slot, compiled. */
    [[nodiscard]] bool make_shaders(Device &made);
    /* Slot `slot` of `chain` drawn into `back` upside down. */
    void               draw_flipped(Chain &chain, Device &device, ID3D11Texture2D *back, uint32_t slot);
    Chain             *chain_of(void *window);
    Chain             *free_chain();
    [[nodiscard]] bool make_chain(Chain &chain, Device &device, uint32_t width, uint32_t height);
    [[nodiscard]] bool make_slots(Chain &chain, Device &device, uint32_t count);
    /* `texture`'s object id and bytes, from the private data its NT handle
     * opens with. */
    [[nodiscard]] bool identify(Device &device, ID3D11Texture2D *texture, uint64_t &id, uint64_t &bytes);
    void               drop_slots(Chain &chain);
    void               drop_chain(Chain &chain);
    void               drop_device(Device &device);
    void               refuse(const char *why);
    /* The first chain opened, and the first present's answer, said once each:
     * whether a window is shown this way at all is otherwise invisible. */
    void               note(const char *what, uint64_t value);

    Kmt             &kmt_;
    icd::ThreadLock &lock_;
    icd::LogSink    &log_;
    void            *d3d11_;
    void            *dxgi_;
    void            *create_device_;
    void            *create_factory_;
    void            *compiler_;
    void            *compile_;
    bool             load_refused_;
    bool             opened_noted_;
    bool             presented_noted_;
    Device           devices_[max_devices];
    Chain            chains_[max_windows];
};

}  // namespace av

#endif  // AV_UM_DXGI_WINDOW_CHAINS_HPP
