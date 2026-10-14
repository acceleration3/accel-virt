/* Where one host GPU's memory comes from (docs/host-memory.md): a Vulkan
 * device that allocates memory exported as a dma-buf - the GPU's compositing
 * context - and the memory types it has. Virtual because there are two real
 * implementations: the compositor's (CompositorMemory) and the mock that
 * makes a GPU with any memory types testable on a machine with none. Every
 * call is serialized by its one caller, HostMemorySource. */
#ifndef AV_MEMORY_EXPORTER_HPP
#define AV_MEMORY_EXPORTER_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class ExportedMemory;

class MemoryExporter {
public:
    MemoryExporter(const MemoryExporter &) = delete;
    MemoryExporter &operator=(const MemoryExporter &) = delete;

    /* How many memory types the GPU has: 0 where there is no device. */
    virtual uint32_t memory_types() = 0;
    /* Type `type`'s VkMemoryPropertyFlags; the caller keeps it below
     * memory_types(). */
    virtual uint32_t memory_flags(uint32_t type) = 0;
    /* Whether memory of `type` is allocated, exported as a dma-buf and mapped
     * through that dma-buf here - what the guest's mapping of it rests on. */
    virtual bool     exportable(uint32_t type) = 0;
    /* `size` bytes of `type`, exported and mapped: ok with `memory` made,
     * no_memory with it empty. */
    [[nodiscard]] virtual Status allocate(uint32_t type, uint64_t size, ExportedMemory &memory) = 0;
    /* What allocate made, gone: its mapping, its descriptor and its memory. */
    virtual void                 free(const ExportedMemory &memory) = 0;
    /* A new descriptor of `memory`'s dma-buf, the caller's: -1 when none. */
    virtual int                  share(const ExportedMemory &memory) = 0;

protected:
    MemoryExporter() = default;
    ~MemoryExporter() = default;
};

}  // namespace av

#endif  // AV_MEMORY_EXPORTER_HPP
