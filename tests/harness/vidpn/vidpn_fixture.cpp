#include "harness/vidpn/vidpn_fixture.hpp"

#include "harness/framework/test_registry.hpp"

namespace av {

VidPnFixture::VidPnFixture() : platform_(), function_(), kernel_(platform_, function_), dxgkrnl_(platform_), vidpn_()
{
    if (dxgkrnl_.initialize(&kernel_.record()) != Status::ok)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "dxgkrnl interface did not initialize");
}

MockPlatform     &VidPnFixture::platform() { return platform_; }
DxgkrnlInterface &VidPnFixture::dxgkrnl() { return dxgkrnl_; }
FakeVidPn        &VidPnFixture::vidpn() { return vidpn_; }
void             *VidPnFixture::handle() { return &vidpn_; }

}  // namespace av
