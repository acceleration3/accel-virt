/* The guest devices this library serves, each a domain: one virtio-gpu-av
 * device - one guest adapter - on one host GPU. The guest's driver mints its
 * shared ids per adapter, so two devices on one GPU would name different
 * objects by the same id; a domain keeps them apart (HostShares).
 *
 * A domain is opened as its device realizes and closed as it unrealizes, on
 * QEMU's main loop; any thread may ask which GPU one is on, so each entry is
 * read and written whole. Ids are never reused: a device that asks with one
 * it closed is told it names nothing. */
#ifndef AV_EXECUTOR_DOMAINS_HPP
#define AV_EXECUTOR_DOMAINS_HPP

#include <stdint.h>

namespace av {

class StateWriter;

class ExecutorDomains {
public:
    /* Devices one QEMU process realizes at once, which is a handful at most:
     * each is a guest adapter with its own PCI function. */
    static constexpr uint32_t max_domains = 16u;

    ExecutorDomains();
    ExecutorDomains(const ExecutorDomains &) = delete;
    ExecutorDomains &operator=(const ExecutorDomains &) = delete;

    /* A new domain on the GPU at `gpu_index`; 0 when every entry is taken. */
    uint32_t open(uint32_t gpu_index);
    /* `domain` closed, with the GPU it was on; false for one not open. */
    bool     close(uint32_t domain, uint32_t &gpu_index);
    /* The GPU `domain` is on; false for one not open, 0 included. */
    bool     gpu_of(uint32_t domain, uint32_t &gpu_index) const;
    uint32_t live() const;
    /* A line per open domain, inside the line the caller opened. */
    void     report(StateWriter &writer) const;

private:
    /* The entry holding `domain`, or max_domains. */
    uint32_t entry_of(uint32_t domain) const;

    /* 0 marks a free entry. */
    uint32_t ids_[max_domains];
    uint32_t gpus_[max_domains];
    uint32_t next_;
};

}  // namespace av

#endif  // AV_EXECUTOR_DOMAINS_HPP
