#ifndef AV_SHM_TRACE_REGION_HPP
#define AV_SHM_TRACE_REGION_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ShmTraceRegion {
public:
    static constexpr uint32_t name_bytes = 64u;

    constexpr ShmTraceRegion() = default;
    ~ShmTraceRegion();
    ShmTraceRegion(const ShmTraceRegion &) = delete;
    ShmTraceRegion &operator=(const ShmTraceRegion &) = delete;

    [[nodiscard]] Status open(const char *module, size_t bytes);
    [[nodiscard]] Status open(const char *prefix, const char *module, size_t bytes);
    uint8_t             *region() const;
    size_t               bytes() const;
    uint32_t             process() const;

private:
    void sweep() const;

    uint8_t *region_ = nullptr;
    size_t   bytes_ = 0u;
    uint32_t process_ = 0u;
};

}

#endif
