/* Where one virtio structure lives: a BAR, and a range inside it
 * (virtio 1.2 4.1.4). */
#ifndef AV_VIRTIO_REGION_HPP
#define AV_VIRTIO_REGION_HPP

#include <stdint.h>

namespace av {

class VirtioRegion {
public:
    /* Absent: the device did not present this structure. */
    VirtioRegion();
    VirtioRegion(uint8_t bar, uint64_t offset, uint64_t length);

    bool     present() const;
    uint8_t  bar() const;
    uint64_t offset() const;
    uint64_t length() const;

private:
    bool     present_;
    uint8_t  bar_;
    uint64_t offset_;
    uint64_t length_;
};

}  // namespace av

#endif  // AV_VIRTIO_REGION_HPP
