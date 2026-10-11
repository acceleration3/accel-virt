/* Two guest contexts of one device, each with its own executor on its own
 * worker thread, in one domain of a mock library that keeps the objects they
 * share (feature 053).
 *
 * Two, not one, because the whole claim of US3 is about what two contexts see
 * of one object, and a rig with one context can only assert bookkeeping. Each
 * context's executor calls must run on that context's thread - the executor
 * ABI says so - so `on_first` and `on_second` are what a test drives them
 * through, and a call made on the wrong one is a HostViolation the mock
 * records. */
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

/* One context of the rig: its executor, and the thread it runs on.
 * A type of its own because a context is a real thing here, not a pair of
 * fields the rig would otherwise have twice. */
class SharedObjectContext {
public:
    SharedObjectContext(MockHost &host, const ExecutorLibrary &library, ExecutorShares &shares, std::uint32_t id);
    ~SharedObjectContext();
    SharedObjectContext(const SharedObjectContext &) = delete;
    SharedObjectContext &operator=(const SharedObjectContext &) = delete;

    /* The worker and its executor. `executor` false leaves the
     * context with no executor at all, which is a context whose ring never
     * attached. */
    [[nodiscard]] Status start(bool executor);
    void                 stop();

    ContextExecutor &executor();

    /* Runs `body` on this context's worker and waits for it. */
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
    /* `second_executor` false leaves the second context's worker running with
     * no executor on it, which is a context whose ring never attached: the
     * device still has it, and everything it is asked answers invalid_state. */
    explicit SharedObjectRig(bool second_executor = true);
    ~SharedObjectRig();
    SharedObjectRig(const SharedObjectRig &) = delete;
    SharedObjectRig &operator=(const SharedObjectRig &) = delete;

    MockHost             &host();
    MockExecutorOps      &ops();
    SharedObjectContext  &first();
    SharedObjectContext  &second();

    /* The executors' calls each stayed on their own thread. */
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

}  // namespace av

#endif  // AV_SHARED_OBJECT_RIG_HPP
