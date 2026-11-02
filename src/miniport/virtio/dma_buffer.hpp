#ifndef AV_DMA_BUFFER_HPP
#define AV_DMA_BUFFER_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct DmaBufferProbe;

class DmaBuffer {
public:
    explicit DmaBuffer(Platform &platform);
    ~DmaBuffer();
    DmaBuffer(const DmaBuffer &) = delete;
    DmaBuffer &operator=(const DmaBuffer &) = delete;

    [[nodiscard]] Status initialize(size_t bytes);

    uint8_t *bytes() const;
    uint64_t physical_address() const;
    size_t   size() const;

    bool invariants_hold() const;

private:
    Platform &platform_;
    uint8_t  *bytes_;
    uint64_t  physical_address_;
    size_t    size_;

    friend struct DmaBufferProbe;
};

}

#endif
