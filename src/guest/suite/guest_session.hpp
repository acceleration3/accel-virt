/* The kernel objects one scenario creates on the adapter - a device, its
 * allocations and their GPU virtual addresses, a paging queue and a context -
 * each step refused until the object it needs exists, and all of them
 * destroyed in reverse by tear_down() or the destructor whatever failed. A
 * handle or address of 0 answered as success is malformed and not kept. */
#ifndef AV_GUEST_SESSION_HPP
#define AV_GUEST_SESSION_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Kmt;

class GuestSession {
public:
    static constexpr uint32_t max_allocations = 2u;

    GuestSession(Kmt &kmt, uint32_t adapter);
    ~GuestSession();
    GuestSession(const GuestSession &) = delete;
    GuestSession &operator=(const GuestSession &) = delete;

    [[nodiscard]] Status open_device();
    /* The slot the allocation took, in `index`. */
    [[nodiscard]] Status allocate(const uint8_t *descriptor, uint32_t size, uint32_t &index);
    [[nodiscard]] Status open_paging_queue();
    /* Resident, then mapped over `pages`, each paging fence awaited. */
    [[nodiscard]] Status map(uint32_t index, uint64_t pages, uint64_t &address);
    [[nodiscard]] Status open_context();
    [[nodiscard]] Status submit(uint64_t commands, uint64_t length);
    [[nodiscard]] Status wait();
    /* The first failure among the destructions; every one is attempted. */
    [[nodiscard]] Status tear_down();
    /* The NTSTATUS of that first failure. */
    uint32_t             teardown_code() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status accept(Status status, uint64_t value) const;
    void                 note(Status &first, Status status);

    Kmt     &kmt_;
    uint32_t adapter_;
    uint32_t device_;
    uint32_t allocations_[max_allocations];
    uint64_t addresses_[max_allocations];
    uint64_t pages_[max_allocations];
    uint32_t allocation_count_;
    uint32_t queue_;
    uint32_t fence_object_;
    uint32_t context_;
    uint32_t teardown_code_;

    friend struct GuestSessionProbe;
};

}  // namespace av

#endif  // AV_GUEST_SESSION_HPP
