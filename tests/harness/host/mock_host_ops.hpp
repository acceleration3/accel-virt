/* The C operations table the QEMU glue fills, filled instead with calls into a
 * MockHost, so TableHost and the RvkDevice ABI run against the adversarial host. */
#ifndef AV_MOCK_HOST_OPS_HPP
#define AV_MOCK_HOST_OPS_HPP

#include "device/abi/rvk_device.h"

namespace av {

class MockHost;

class MockHostOps {
public:
    explicit MockHostOps(MockHost &host);
    MockHostOps(const MockHostOps &) = delete;
    MockHostOps &operator=(const MockHostOps &) = delete;

    const RvkHostOps &table() const;

private:
    RvkHostOps table_;
};

}  // namespace av

#endif  // AV_MOCK_HOST_OPS_HPP
