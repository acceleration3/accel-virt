/* One allocation of host GPU memory as a memory exporter made it: the
 * VkDeviceMemory, the dma-buf it was exported as, the exporter's own CPU
 * mapping of it, and its size. A value; the exporter that made it frees it. */
#ifndef AV_EXPORTED_MEMORY_HPP
#define AV_EXPORTED_MEMORY_HPP

#include <stdint.h>

namespace av {

class ExportedMemory {
public:
    /* Nothing. */
    ExportedMemory();
    ExportedMemory(uint64_t handle, int fd, uint8_t *pointer, uint64_t size);

    bool     made() const;
    /* The VkDeviceMemory, as the one number every handle fits in. */
    uint64_t handle() const;
    int      fd() const;
    uint8_t *pointer() const;
    uint64_t size() const;

private:
    uint64_t handle_;
    int      fd_;
    uint8_t *pointer_;
    uint64_t size_;
};

}  // namespace av

#endif  // AV_EXPORTED_MEMORY_HPP
