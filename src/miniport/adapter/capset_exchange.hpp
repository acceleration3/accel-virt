#ifndef AV_CAPSET_EXCHANGE_HPP
#define AV_CAPSET_EXCHANGE_HPP

#include "core/platform/platform.hpp"
#include "miniport/adapter/capset_query.hpp"
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

    [[nodiscard]] Status read(CapsetInfo &capset);

private:
    Platform        &platform_;
    VirtioGpuDevice &device_;
    CapsetQuery      query_;
    uint8_t          request_[AvRequestWriter::max_request_size];
    uint8_t          answer_[GpuRequestWriter::max_response_size];
};

}

#endif
