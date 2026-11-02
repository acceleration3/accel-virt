#include "harness/executor/root_under_test.hpp"

#include "core/support/object_storage.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/executor_root.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstring>

namespace av {

namespace {
ObjectStorage<ExecutorRoot> storage;

}

RootUnderTest::RootUnderTest() : held_(false), gpus_{}, domains_{}
{
    AV_SCOPE("RootUnderTest::RootUnderTest");
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

std::uint32_t RootUnderTest::domain_of(const std::uint8_t *gpu_uuid)
{
    AV_SCOPE("RootUnderTest::domain_of");
    if (gpu_uuid == nullptr) return 0u;
    for (std::uint32_t entry = 0u; entry < max_domains; entry++)
        if (domains_[entry] != 0u && std::memcmp(gpus_[entry], gpu_uuid, sizeof gpus_[entry]) == 0)
            return domains_[entry];
    const std::uint32_t opened = storage.object().open_domain(gpu_uuid);
    for (std::uint32_t entry = 0u; entry < max_domains && opened != 0u; entry++) {
        if (domains_[entry] != 0u) continue;
        std::memcpy(gpus_[entry], gpu_uuid, sizeof gpus_[entry]);
        domains_[entry] = opened;
        break;
    }
    return opened;
}

ContextSession *RootUnderTest::open(std::uint32_t sub_api, const std::uint8_t *gpu_uuid,
                                    const AvContextOptions *options, ContextSession *share)
{
    AV_SCOPE("RootUnderTest::open");
    AvContextOptions asked = options != nullptr ? *options : AvContextOptions{};
    asked.domain = domain_of(gpu_uuid);
    return storage.object().open(sub_api, asked, share);
}

bool RootUnderTest::invariants_hold() const
{
    AV_SCOPE("RootUnderTest::invariants_hold");
    return held_ == storage.constructed();
}

}
