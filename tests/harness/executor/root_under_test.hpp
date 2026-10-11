/* One test's ExecutorRoot, kept off the stack.
 *
 * The root is eight megabytes of fixed-size session tables - sixty-four
 * sessions, each holding every backend it might become - and a test that
 * declares one as a local overflows an eight-megabyte stack before the first
 * assertion runs. The device never does that: `av_executor_ops()` builds its
 * root in an ObjectStorage in the entry point's translation unit, because a
 * C entry point has nowhere to put a `this`.
 *
 * This is that same storage, borrowed for a test: static rather than
 * automatic, constructed and destroyed around one test so each gets a root
 * that has seen nothing. One at a time, which is what every test needs and
 * what the constructor checks. */
#ifndef AV_ROOT_UNDER_TEST_HPP
#define AV_ROOT_UNDER_TEST_HPP

#include "device/abi/av_executor.h"

#include <cstdint>

namespace av {

class ContextSession;
class ExecutorRoot;

class RootUnderTest {
public:
    RootUnderTest();
    ~RootUnderTest();
    RootUnderTest(const RootUnderTest &) = delete;
    RootUnderTest &operator=(const RootUnderTest &) = delete;

    ExecutorRoot &root() const;
    /* So a test reads as it did when the root was a local. */
    ExecutorRoot *operator->() const;
    ExecutorRoot &operator*() const;

    /* The domain a device on the GPU `gpu_uuid` names would have, opened the
     * first time it is asked for and the same one after; 0 for a GPU the
     * root has not got. */
    std::uint32_t   domain_of(const std::uint8_t *gpu_uuid);
    /* A session of `sub_api` in that domain, as a device on the GPU makes one:
     * what a test of the root opens by GPU. */
    ContextSession *open(std::uint32_t sub_api, const std::uint8_t *gpu_uuid,
                         const AvContextOptions *options = nullptr, ContextSession *share = nullptr);

    bool invariants_hold() const;

private:
    static constexpr std::uint32_t max_domains = 8u;

    bool          held_;
    std::uint8_t  gpus_[max_domains][16];
    std::uint32_t domains_[max_domains];
};

}  // namespace av

#endif  // AV_ROOT_UNDER_TEST_HPP
