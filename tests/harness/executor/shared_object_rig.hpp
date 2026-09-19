/* Two guest contexts of one device, each with its own executor on its own
 * worker thread, over one device-global table of shared objects (feature 053).
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
#include "device/execution/context_shared_objects.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/memory/host_shared_objects.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

namespace av {

/* One context of the rig: its executor, its table, and the thread both run on.
 * A type of its own because a context is a real thing here, not a pair of
 * fields the rig would otherwise have twice. */
class SharedObjectContext {
public:
    SharedObjectContext(MockHost &host, const ExecutorLibrary &library, ExecutorShares &shares,
                        HostSharedObjects &objects, std::uint32_t id);
    ~SharedObjectContext();
    SharedObjectContext(const SharedObjectContext &) = delete;
    SharedObjectContext &operator=(const SharedObjectContext &) = delete;

    /* The worker, its executor and its table. `executor` false leaves the
     * context with no executor at all, which is a context whose ring never
     * attached. */
    [[nodiscard]] Status start(bool executor);
    void                 stop();

    ContextExecutor      &executor();
    ContextSharedObjects &objects();

    /* Runs `body` on this context's worker and waits for it. */
    template <typename Body>
    void on_worker(Body body);

private:
    void run_job(void (*job)(void *), void *context);
    void serve();

    MockHost            &host_;
    ContextExecutor      executor_;
    ContextSharedObjects objects_;
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
    HostSharedObjects    &shared();
    SharedObjectContext  &first();
    SharedObjectContext  &second();

    /* Both tables' invariants and the device's, which a test asserts after
     * every mutation. */
    bool invariants_hold() const;

private:
    MockHost            host_;
    MockExecutorOps     ops_;
    ExecutorLibrary     library_;
    ExecutorShares      shares_;
    HostSharedObjects   objects_;
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
