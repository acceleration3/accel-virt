/* The memory behind one host-visible blob: a block the host provides (a memfd
 * now, GPU memory with execution) that can be placed in the hostmem region for
 * the guest to map. */
#ifndef AV_HOST_MEMORY_BLOCK_HPP
#define AV_HOST_MEMORY_BLOCK_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlobWindow;
class Host;

class HostMemoryBlock {
public:
    static constexpr uint64_t page_size = 4096u;

    explicit HostMemoryBlock(Host &host);
    ~HostMemoryBlock();
    HostMemoryBlock(const HostMemoryBlock &) = delete;
    HostMemoryBlock &operator=(const HostMemoryBlock &) = delete;

    /* Main loop. invalid_parameter for an empty or partial-page size;
     * no_memory when the host has none; invalid_state when already created. */
    [[nodiscard]] Status create(uint64_t size);
    void                 release();
    bool                 created() const;
    uint64_t             size() const;
    /* The host's handle, for placing the block. */
    void                *handle() const;
    /* The device's own view of the block; empty until created. */
    BlobWindow           window() const;

private:
    Host    &host_;
    void    *handle_;
    void    *view_;
    uint64_t size_;
};

}  // namespace av

#endif  // AV_HOST_MEMORY_BLOCK_HPP
