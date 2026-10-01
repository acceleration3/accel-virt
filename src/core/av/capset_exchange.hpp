/* Reads a device's av capset over its control queue: CapsetQuery says what to
 * ask, the device answers, until the capset is described or the first failure
 * (docs/protocol.md). Passive only - each answer is waited for - and one read
 * per object: an exchange that failed stays failed. */
#ifndef AV_CAPSET_EXCHANGE_HPP
#define AV_CAPSET_EXCHANGE_HPP

#include "core/platform/platform.hpp"
#include "core/av/capset_query.hpp"
#include "core/support/status.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"

#include <stdint.h>

namespace av {

class CapsetInfo;
class VirtioGpuDevice;

class CapsetExchange {
public:
    CapsetExchange(Platform &platform, VirtioGpuDevice &device);
    CapsetExchange(const CapsetExchange &) = delete;
    CapsetExchange &operator=(const CapsetExchange &) = delete;

    /* `capset` is left alone unless this returns ok. */
    [[nodiscard]] Status read(CapsetInfo &capset);

private:
    Platform        &platform_;
    VirtioGpuDevice &device_;
    CapsetQuery      query_;
    uint8_t          request_[AvRequestWriter::max_request_size];
    uint8_t          answer_[GpuRequestWriter::max_response_size];
};

}  // namespace av

#endif  // AV_CAPSET_EXCHANGE_HPP
