/* One test's ExecutorRoot, kept off the stack.
 *
 * The root is eight megabytes of fixed-size session tables - sixty-four
 * sessions, each holding every backend it might become - and a test that
 * declares one as a local overflows an eight-megabyte stack before the first
 * assertion runs. The device never does that: `rvk_executor_ops()` builds its
 * root in an ObjectStorage in the entry point's translation unit, because a
 * C entry point has nowhere to put a `this`.
 *
 * This is that same storage, borrowed for a test: static rather than
 * automatic, constructed and destroyed around one test so each gets a root
 * that has seen nothing. One at a time, which is what every test needs and
 * what the constructor checks. */
#ifndef AV_ROOT_UNDER_TEST_HPP
#define AV_ROOT_UNDER_TEST_HPP

namespace av {

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

    bool invariants_hold() const;

private:
    bool held_;
};

}  // namespace av

#endif  // AV_ROOT_UNDER_TEST_HPP
