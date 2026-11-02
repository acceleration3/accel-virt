#ifndef AV_VIRT_QUEUE_HPP
#define AV_VIRT_QUEUE_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/dma_buffer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class VirtqSegment;
struct VirtQueueProbe;

class VirtQueue {
public:
    static constexpr uint16_t max_size = 1024u;

    VirtQueue(Platform &platform, uint16_t index);
    ~VirtQueue();
    VirtQueue(const VirtQueue &) = delete;
    VirtQueue &operator=(const VirtQueue &) = delete;

    [[nodiscard]] Status initialize(uint16_t device_max_size);
    void                 bind_notify_offset(uint64_t offset);

    [[nodiscard]] Status add(const VirtqSegment *segments, uint16_t count, uint16_t &head);
    [[nodiscard]] Status take_used(uint16_t &head, uint32_t &written);

    uint16_t index() const;
    uint16_t size() const;
    uint16_t free_descriptors() const;
    uint64_t descriptor_area() const;
    uint64_t driver_area() const;
    uint64_t device_area() const;
    uint64_t notify_offset() const;

    bool invariants_hold() const;

private:
    struct Shadow;

    uint16_t read_u16(size_t offset) const;
    uint32_t read_u32(size_t offset) const;
    void     publish_u16(size_t offset, uint16_t value);
    uint16_t observe_u16(size_t offset) const;
    void     write_u16(size_t offset, uint16_t value);
    void     write_u32(size_t offset, uint32_t value);
    void     write_u64(size_t offset, uint64_t value);
    void     free_chain(uint16_t head);

    Platform &platform_;
    uint16_t  index_;
    uint16_t  size_;
    DmaBuffer ring_;
    Shadow   *shadow_;
    uint16_t  free_head_;
    uint16_t  free_count_;
    uint16_t  avail_index_;
    uint16_t  last_used_index_;
    size_t    driver_offset_;
    size_t    device_offset_;
    uint64_t  notify_offset_;

    friend struct VirtQueueProbe;
};

}

#endif
