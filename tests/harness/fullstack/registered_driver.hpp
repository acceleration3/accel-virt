#ifndef AV_REGISTERED_DRIVER_HPP
#define AV_REGISTERED_DRIVER_HPP

#include "miniport/driver/ddi_table.hpp"
#include "miniport/driver/driver_image.hpp"
#include "core/support/status.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>

namespace av {

class RegisteredDriver {
public:
    RegisteredDriver();
    ~RegisteredDriver();
    RegisteredDriver(const RegisteredDriver &) = delete;
    RegisteredDriver &operator=(const RegisteredDriver &) = delete;

    std::int32_t    entered() const;
    Status          registered() const;
    MockPlatform   &platform();
    const DdiTable &table();

private:
    int                       driver_object_;
    int                       registry_path_;
    DriverImage<MockPlatform, MockTraceHost> image_;
    std::int32_t              entered_;
    FakeDxgkrnl               dxgkrnl_;
    Status                    registered_;
};

}

#endif
