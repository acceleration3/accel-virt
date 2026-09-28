/* The order a device runs its Direct3D streams' batches in: a batch runs only
 * after every other stream's batches the guest published before it. The guest
 * stamps each batch with its performance counter as it writes it, so a
 * process's frame, published before its present, runs before the DWM batch
 * that composes it, published after DWM learned of the present - with no
 * round trip on the way. Waits point back in time, so they never form a
 * cycle among the streams; the cap is for a stream whose guest stopped
 * mid-publish.
 *
 * A stream waited on is woken to run what it holds. A process publishes its
 * own tail, so a batch can be in its ring with no doorbell coming: the kernel
 * rings that only when dxgkrnl hands the packet over, and dxgkrnl holds one
 * context's packets while another's are still running - which the waiting
 * stream's own are, until its batch runs. Waiting for the doorbell would be
 * a wait on itself, and would end only at the cap. */
#ifndef AV_D3D_ORDERING_HPP
#define AV_D3D_ORDERING_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
class RingConsumer;
class Worker;

class D3dOrdering {
public:
    static constexpr uint32_t capacity = 64u;

    explicit D3dOrdering(Host &host);
    ~D3dOrdering();
    D3dOrdering(const D3dOrdering &) = delete;
    D3dOrdering &operator=(const D3dOrdering &) = delete;

    [[nodiscard]] Status initialize();
    /* `worker` serves `ring` and keeps running until `ring` is removed: a
     * stream waiting on `ring` wakes it from its own thread. */
    void                 add(RingConsumer &ring, Worker &worker);
    void                 remove(RingConsumer &ring);
    /* On `ring`'s worker, before it runs the batch published at `published_at`. */
    void                 wait_before(const RingConsumer &ring, uint64_t published_at);
    /* Before the scanout reads an image: every stream has run what it published. */
    void                 settle();

private:
    struct Member {
        RingConsumer *ring_;
        Worker       *worker_;
    };

    /* Wakes every other stream holding a batch published before
     * `published_at`, and answers whether there was one. */
    bool wake_earlier(const RingConsumer *ring, uint64_t published_at);
    bool must_wait(const RingConsumer *ring, uint64_t published_at);
    void pause() const;

    Host    &host_;
    void    *mutex_;
    Member   members_[capacity];
    uint32_t count_;
};

}  // namespace av

#endif  // AV_D3D_ORDERING_HPP
