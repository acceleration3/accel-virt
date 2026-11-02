#ifndef AV_MOCK_HOST_OPS_HPP
#define AV_MOCK_HOST_OPS_HPP

#include "device/abi/av_device.h"

namespace av {

class MockHost;

class MockHostOps {
public:
    explicit MockHostOps(MockHost &host);
    MockHostOps(const MockHostOps &) = delete;
    MockHostOps &operator=(const MockHostOps &) = delete;

    const AvHostOps &table() const;

private:
    AvHostOps table_;
};

}

#endif
