#ifndef AV_UM_DXGI_WINDOW_CHAINS_HPP
#define AV_UM_DXGI_WINDOW_CHAINS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "icd/common/window_chains.hpp"
#include "um/common/com_reference.hpp"

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
class BlockSource;
class KeyedLog;
}

namespace av {

class Kmt;
class ThreadLock;

class DxgiWindowChains final : public icd::WindowChains {
public:
    static constexpr uint32_t max_devices = AvgpuAdapters::max_adapters;

    DxgiWindowChains(BlockSource &memory, Kmt &kmt, ThreadLock &lock, KeyedLog &log);
    ~DxgiWindowChains();
    DxgiWindowChains(const DxgiWindowChains &) = delete;
    DxgiWindowChains &operator=(const DxgiWindowChains &) = delete;

    bool open(void *window, uint64_t luid, uint32_t width, uint32_t height, icd::SlotLayout layout, uint32_t slots,
              uint64_t *ids, uint64_t *bytes) override;
    bool present(void *window, uint32_t slot) override;
    bool settled(void *window, uint32_t slot) override;
    void close(void *window) override;

private:
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
    struct Chain {
        void                                   *window;
        uint32_t                                device;
        ComReference<IDXGISwapChain1>           chain;
        ComReference<ID3D11Texture2D>           slots[max_slots];
        ComReference<ID3D11ShaderResourceView>  views[max_slots];
        uint64_t                                ids[max_slots];
        uint64_t                                bytes[max_slots];
        ComReference<ID3D11Query>               copied[max_slots];
        uint32_t                                width;
        uint32_t                                height;
        uint32_t                                count;
        icd::SlotLayout                         layout;
        uint32_t                                format;
    };

    [[nodiscard]] bool loaded();
    uint32_t           device_for(uint64_t luid);
    [[nodiscard]] bool make_device(uint64_t luid, Device &made);
    [[nodiscard]] bool kernel_device(uint64_t luid, Device &made);
    [[nodiscard]] bool make_shaders(Device &made);
    void               draw_flipped(Chain &chain, Device &device, ID3D11Texture2D *back, uint32_t slot);
    Chain             *chain_of(void *window);
    Chain             *new_chain(void *window);
    void               forget_chain(void *window);
    [[nodiscard]] bool make_chain(Chain &chain, Device &device, uint32_t width, uint32_t height);
    [[nodiscard]] bool make_slots(Chain &chain, Device &device, uint32_t count);
    void               drop_slots(Chain &chain);
    void               drop_chain(Chain &chain);
    void               drop_device(Device &device);
    void               refuse(const char *why);

    Kmt             &kmt_;
    ThreadLock      &lock_;
    KeyedLog        &lines_;
    void            *d3d11_;
    void            *dxgi_;
    void            *create_device_;
    void            *create_factory_;
    void            *compiler_;
    void            *compile_;
    bool             load_refused_;
    Device           devices_[max_devices];
    ObjectPool<Chain> chains_;
    KeyedSlots<uint64_t, Chain *> windows_;
};

}

#endif
