/* What the renderer's service asks of the process beyond what the device
 * core asks of any host (docs/renderer.md): the descriptors QEMU passes, which
 * only the process can map, keep or close, and the executor library it loads.
 * The process answers it for real; a test answers it from a script. */
#ifndef AV_RENDERER_RESOURCES_HPP
#define AV_RENDERER_RESOURCES_HPP

#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"

#include <stdint.h>

namespace av {

class RendererResources {
public:
    /* The whole of guest RAM region `descriptor`, shared with QEMU's own
     * mapping: no_memory when it cannot be mapped. */
    [[nodiscard]] virtual Status map_region(int descriptor, uint64_t size, void *&base) = 0;
    virtual void                 unmap_region(void *base, uint64_t size) = 0;
    virtual void                 close_descriptor(int descriptor) = 0;
    /* The memfd the next host blob is made over, which the resources close. */
    virtual void                 offer_host_memory(int descriptor) = 0;
    /* The guest notifiers, one per fence line, -1 for a line with none; the
     * resources close them when they are replaced or dropped. */
    virtual void                 take_interrupt_lines(const int *descriptors, uint32_t count) = 0;
    virtual void                 drop_interrupt_lines() = 0;
    /* The table the library at `path` (`length` bytes, not terminated)
     * answers: not_found when it does not open, not_supported when it answers
     * none. */
    [[nodiscard]] virtual Status load_executor(const char *path, uint32_t length, const RvkExecutorOps *&ops) = 0;

protected:
    RendererResources() = default;
    ~RendererResources() = default;
    RendererResources(const RendererResources &) = delete;
    RendererResources &operator=(const RendererResources &) = delete;
};

}  // namespace av

#endif  // AV_RENDERER_RESOURCES_HPP
