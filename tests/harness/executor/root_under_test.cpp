#include "harness/executor/root_under_test.hpp"

#include "core/support/object_storage.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/executor_root.hpp"
#include "harness/framework/test_registry.hpp"

namespace av {

namespace {

/* The storage itself. Static because the whole point is that it is not on the
 * stack, and one because no test has ever needed two roots at once. */
ObjectStorage<ExecutorRoot> storage;

}  // namespace

RootUnderTest::RootUnderTest() : held_(false)
{
    AV_SCOPE("RootUnderTest::RootUnderTest");
    /* A second one alive at the same time would hand both tests the same
     * root, which is worse than refusing. */
    CHECK(!storage.constructed());
    if (storage.constructed()) return;
    CHECK_STATUS(storage.construct(), Status::ok);
    held_ = true;
}

RootUnderTest::~RootUnderTest()
{
    AV_SCOPE("RootUnderTest::~RootUnderTest");
    if (held_) storage.destroy();
}

ExecutorRoot &RootUnderTest::root() const
{
    AV_SCOPE("RootUnderTest::root");
    return storage.object();
}

ExecutorRoot *RootUnderTest::operator->() const
{
    AV_SCOPE("RootUnderTest::operator->");
    return &storage.object();
}

ExecutorRoot &RootUnderTest::operator*() const
{
    AV_SCOPE("RootUnderTest::operator*");
    return storage.object();
}

bool RootUnderTest::invariants_hold() const
{
    AV_SCOPE("RootUnderTest::invariants_hold");
    return held_ == storage.constructed();
}

}  // namespace av
