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
    void                 add(RingConsumer &ring, Worker &worker);
    void                 remove(RingConsumer &ring);
    void                 wait_before(const RingConsumer &ring, uint64_t published_at);
    void                 settle();

private:
    struct Member {
        RingConsumer *ring_;
        Worker       *worker_;
    };

    bool wake_earlier(const RingConsumer *ring, uint64_t published_at);
    bool must_wait(const RingConsumer *ring, uint64_t published_at);
    void pause() const;

    Host    &host_;
    void    *mutex_;
    Member   members_[capacity];
    uint32_t count_;
};

}

#endif
