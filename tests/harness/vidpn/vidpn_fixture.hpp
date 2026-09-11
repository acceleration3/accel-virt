/* What a VidPn DDI needs from dxgkrnl, faked: a platform, an initialized
 * dxgkrnl interface whose DxgkCbQueryVidPnInterface answers a FakeVidPn, and
 * that VidPn, whose leak check runs after anything a test declares. */
#ifndef AV_VIDPN_FIXTURE_HPP
#define AV_VIDPN_FIXTURE_HPP

#include "core/miniport/dxgkrnl_interface.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
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
    /* The hVidPn dxgkrnl would pass. */
    void             *handle();

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    kernel_;
    DxgkrnlInterface      dxgkrnl_;
    FakeVidPn             vidpn_;
};

}  // namespace av

#endif  // AV_VIDPN_FIXTURE_HPP
