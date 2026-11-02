#ifndef AV_RENDERER_RESOURCES_HPP
#define AV_RENDERER_RESOURCES_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"

#include <stdint.h>

namespace av {

class RendererResources {
public:
    [[nodiscard]] virtual Status map_region(int descriptor, uint64_t size, void *&base) = 0;
    virtual void                 unmap_region(void *base, uint64_t size) = 0;
    virtual void                 close_descriptor(int descriptor) = 0;
    virtual void                 take_interrupt_lines(const int *descriptors, uint32_t count) = 0;
    virtual void                 drop_interrupt_lines() = 0;
    [[nodiscard]] virtual Status load_executor(const char *path, uint32_t length, const AvExecutorOps *&ops) = 0;

protected:
    RendererResources() = default;
    ~RendererResources() = default;
    RendererResources(const RendererResources &) = delete;
    RendererResources &operator=(const RendererResources &) = delete;
};

}

#endif
