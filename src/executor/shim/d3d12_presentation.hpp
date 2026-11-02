#ifndef AV_D3D12_PRESENTATION_HPP
#define AV_D3D12_PRESENTATION_HPP

#include <stdint.h>
#include "executor/shim/compose_order.hpp"

#include <vulkan/vulkan.h>

struct ID3D12CommandQueue;
struct ID3D12Fence;
struct ID3D12Resource;

namespace av {

class D3d11HostDevice;
class DriverLog;
class D3d12HostDevice;
class SessionShares;

class D3d12Presentation {
public:
    explicit D3d12Presentation(DriverLog &lines);
    ~D3d12Presentation();
    D3d12Presentation(const D3d12Presentation &) = delete;
    D3d12Presentation &operator=(const D3d12Presentation &) = delete;

    void               bind(D3d12HostDevice *device12, D3d11HostDevice *device11, SessionShares *shares);
    [[nodiscard]] bool share(ID3D12Resource *resource, uint64_t key);
    [[nodiscard]] bool present(ID3D12CommandQueue *queue);
    void               release();

private:
    [[nodiscard]] bool connect();

    DriverLog       &lines_;
    D3d12HostDevice *device12_;
    D3d11HostDevice *device11_;
    SessionShares   *shares_;
    ID3D12Fence     *fence_;
    ComposeOrder     order_;
};

}

#endif
