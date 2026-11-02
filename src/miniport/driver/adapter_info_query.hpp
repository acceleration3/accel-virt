#ifndef AV_ADAPTER_INFO_QUERY_HPP
#define AV_ADAPTER_INFO_QUERY_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct DxgkargQueryAdapterInfoRecord;

class AdapterInfoQuery {
public:
    AdapterInfoQuery(Platform &platform, const DxgkargQueryAdapterInfoRecord &record);
    AdapterInfoQuery(const AdapterInfoQuery &) = delete;
    AdapterInfoQuery &operator=(const AdapterInfoQuery &) = delete;

    int32_t type() const;
    [[nodiscard]] Status require_input(uint32_t bytes) const;
    [[nodiscard]] Status read_input(void *destination, uint32_t count) const;
    [[nodiscard]] Status read_output(void *destination, uint32_t count) const;
    [[nodiscard]] Status write_output(const void *bytes, uint32_t count) const;
    uint32_t             output_size() const;

private:
    Platform                            &platform_;
    const DxgkargQueryAdapterInfoRecord &record_;
};

}

#endif
