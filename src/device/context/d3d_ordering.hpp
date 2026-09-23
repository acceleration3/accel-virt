/* The order a device runs its Direct3D streams' batches in: a batch runs only
 * after every other stream's batches the guest published before it. The guest
 * stamps each batch with its performance counter as it writes it, so a
 * process's frame, published before its present, runs before the DWM batch
 * that composes it, published after DWM learned of the present - with no
 * round trip on the way. Waits point back in time, so they never form a
 * cycle; the cap is for a stream whose guest stopped mid-publish. */
#ifndef AV_D3D_ORDERING_HPP
#define AV_D3D_ORDERING_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
class RingConsumer;

class D3dOrdering {
public:
    static constexpr uint32_t capacity = 64u;

    explicit D3dOrdering(Host &host);
    ~D3dOrdering();
    D3dOrdering(const D3dOrdering &) = delete;
    D3dOrdering &operator=(const D3dOrdering &) = delete;

    [[nodiscard]] Status initialize();
    void                 add(RingConsumer &ring);
    void                 remove(RingConsumer &ring);
    /* On `ring`'s worker, before it runs the batch published at `published_at`. */
    void                 wait_before(const RingConsumer &ring, uint64_t published_at);
    /* Before the scanout reads an image: every stream has run what it published. */
    void                 settle();

private:
    bool earlier_pending(const RingConsumer *ring, uint64_t published_at) const;
    bool must_wait(const RingConsumer *ring, uint64_t published_at);
    void pause() const;

    Host         &host_;
    void         *mutex_;
    RingConsumer *members_[capacity];
    uint32_t      count_;
};

}  // namespace av

#endif  // AV_D3D_ORDERING_HPP
