#ifndef AV_GUEST_SESSION_HPP
#define AV_GUEST_SESSION_HPP

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

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
    [[nodiscard]] Status allocate(const uint8_t *descriptor, uint32_t size, uint32_t &index);
    [[nodiscard]] Status lock(uint32_t index, void *&view);
    [[nodiscard]] Status unlock(uint32_t index);
    [[nodiscard]] Status open_paging_queue();
    [[nodiscard]] Status map(uint32_t index, uint64_t pages, uint64_t &address);
    [[nodiscard]] Status open_context();
    [[nodiscard]] Status submit(uint64_t commands, uint64_t length);
    [[nodiscard]] Status wait();
    [[nodiscard]] Status await_vertical_blank(uint32_t source);
    [[nodiscard]] Status tear_down();
    uint32_t             teardown_code() const;

    bool invariants_hold() const;

private:
    template <typename Handle>
    [[nodiscard]] Status accept(Status status, const Handle &value) const
    {
        AV_SCOPE("GuestSession::accept");
        return status == Status::ok && value == Handle{} ? Status::malformed : status;
    }
    void                 note(Status &first, Status status);

    Kmt     &kmt_;
    uint32_t adapter_;
    uint32_t device_;
    uint32_t allocations_[max_allocations];
    void    *views_[max_allocations];
    uint64_t addresses_[max_allocations];
    uint64_t pages_[max_allocations];
    uint32_t allocation_count_;
    uint32_t queue_;
    uint32_t fence_object_;
    uint32_t context_;
    uint32_t teardown_code_;

    friend struct GuestSessionProbe;
};

}

#endif
