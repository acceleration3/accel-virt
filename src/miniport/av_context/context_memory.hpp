#ifndef AV_CONTEXT_MEMORY_HPP
#define AV_CONTEXT_MEMORY_HPP

#include "miniport/memory/guest_pages.hpp"
#include "core/platform/platform.hpp"
#include "miniport/av_context/context_pages.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct ContextMemoryProbe;

class ContextMemory {
public:
    explicit ContextMemory(Platform &platform);
    ~ContextMemory();
    ContextMemory(const ContextMemory &) = delete;
    ContextMemory &operator=(const ContextMemory &) = delete;

    [[nodiscard]] Status allocate(uint8_t order, uint32_t reply_bytes);
    [[nodiscard]] Status map_into_process();
    void                 release();
    void                 abandon();

    bool         allocated() const;
    uint8_t      order() const;
    uint64_t     ring_view() const;
    uint64_t     reply_view() const;
    uint64_t     fence_view() const;
    ContextPages pages() const;

    bool invariants_hold() const;

private:
    void unmap_kernel_views();
    void free_all();

    Platform  &platform_;
    GuestPages ring_;
    GuestPages fence_;
    GuestPages reply_;
    uint8_t   *ring_header_;
    uint8_t   *fence_view_;
    uint8_t    order_;

    friend struct ContextMemoryProbe;
};

}

#endif
