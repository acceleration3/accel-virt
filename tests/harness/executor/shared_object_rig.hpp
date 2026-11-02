#ifndef AV_SHARED_OBJECT_RIG_HPP
#define AV_SHARED_OBJECT_RIG_HPP

#include "core/support/status.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

namespace av {
class SharedObjectContext {
public:
    SharedObjectContext(MockHost &host, const ExecutorLibrary &library, ExecutorShares &shares, std::uint32_t id);
    ~SharedObjectContext();
    SharedObjectContext(const SharedObjectContext &) = delete;
    SharedObjectContext &operator=(const SharedObjectContext &) = delete;

    [[nodiscard]] Status start(bool executor);
    void                 stop();

    ContextExecutor &executor();

    template <typename Body>
    void on_worker(Body body);

private:
    void run_job(void (*job)(void *), void *context);
    void serve();

    MockHost            &host_;
    ContextExecutor      executor_;
    void                *start_;
    void                *done_;
    void                *thread_;
    void               (*job_)(void *);
    void                *job_context_;
    bool                 stopping_;
};

class SharedObjectRig {
public:
    explicit SharedObjectRig(bool second_executor = true);
    ~SharedObjectRig();
    SharedObjectRig(const SharedObjectRig &) = delete;
    SharedObjectRig &operator=(const SharedObjectRig &) = delete;

    MockHost             &host();
    MockExecutorOps      &ops();
    SharedObjectContext  &first();
    SharedObjectContext  &second();

    bool invariants_hold();

private:
    MockHost            host_;
    MockExecutorOps     ops_;
    ExecutorLibrary     library_;
    ExecutorShares      shares_;
    SharedObjectContext first_;
    SharedObjectContext second_;
};

template <typename Body>
void SharedObjectContext::on_worker(Body body)
{
    run_job([](void *context) { (*static_cast<Body *>(context))(); }, &body);
}

}

#endif
