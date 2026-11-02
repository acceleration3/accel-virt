#ifndef AV_VIDPN_FIXTURE_HPP
#define AV_VIDPN_FIXTURE_HPP

#include "miniport/driver/dxgkrnl_interface.hpp"
#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/vidpn/fake_vidpn.hpp"

namespace av {

class VidPnFixture {
public:
    VidPnFixture();
    VidPnFixture(const VidPnFixture &) = delete;
    VidPnFixture &operator=(const VidPnFixture &) = delete;

    MockPlatform     &platform();
    DxgkrnlInterface &dxgkrnl();
    FakeVidPn        &vidpn();
    void             *handle();

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    kernel_;
    DxgkrnlInterface      dxgkrnl_;
    FakeVidPn             vidpn_;
};

}

#endif
