#include "harness/executor/shared_object_rig.hpp"

#include "proto/context/gl_context_options.hpp"
#include "proto/context/sub_api.hpp"

#include <cstdlib>

namespace av {

namespace {

constexpr std::uint8_t no_uuid[ContextExecutor::uuid_bytes] = {};

}  // namespace

SharedObjectContext::SharedObjectContext(MockHost &host, const ExecutorLibrary &library,
                                         ExecutorShares &shares, HostSharedObjects &objects,
                                         std::uint32_t id)
    : host_(host), executor_(host, library, shares, id), objects_(objects, executor_), start_(nullptr),
      done_(nullptr), thread_(nullptr), job_(nullptr), job_context_(nullptr), stopping_(false)
{}

SharedObjectContext::~SharedObjectContext()
{
    stop();
}

Status SharedObjectContext::start(bool executor)
{
    if (host_.create_event(start_) != Status::ok || host_.create_event(done_) != Status::ok ||
        host_.start_thread([](void *context) { static_cast<SharedObjectContext *>(context)->serve(); }, this,
                           thread_) != Status::ok)
        return Status::no_memory;
    executor_.configure(SubApi::vulkan, no_uuid, GlContextOptions());
    if (executor) on_worker([this] { static_cast<void>(executor_.create()); });
    return Status::ok;
}

void SharedObjectContext::stop()
{
    if (thread_ == nullptr) return;
    on_worker([this] { executor_.destroy(); });
    objects_.release_all();
    stopping_ = true;
    host_.signal_event(start_);
    host_.join_thread(thread_);
    host_.destroy_event(done_);
    host_.destroy_event(start_);
    thread_ = nullptr;
}

ContextExecutor      &SharedObjectContext::executor() { return executor_; }
ContextSharedObjects &SharedObjectContext::objects() { return objects_; }

void SharedObjectContext::run_job(void (*job)(void *), void *context)
{
    job_ = job;
    job_context_ = context;
    host_.signal_event(start_);
    host_.wait_event(done_);
}

void SharedObjectContext::serve()
{
    for (host_.wait_event(start_); !stopping_; host_.wait_event(start_)) {
        job_(job_context_);
        host_.signal_event(done_);
    }
}

SharedObjectRig::SharedObjectRig(bool second_executor)
    : host_(), ops_(AV_EXECUTOR_SUB_API_VULKAN), library_(), shares_(host_, library_), objects_(host_),
      first_(host_, library_, shares_, objects_, 1u), second_(host_, library_, shares_, objects_, 2u)
{
    ops_.descriptors_from(host_);
    if (library_.accept(&ops_.table()) != Status::ok || objects_.initialize() != Status::ok ||
        first_.start(true) != Status::ok || second_.start(second_executor) != Status::ok)
        std::abort();
}

SharedObjectRig::~SharedObjectRig()
{
    second_.stop();
    first_.stop();
    objects_.release_all();
}

MockHost            &SharedObjectRig::host() { return host_; }
MockExecutorOps     &SharedObjectRig::ops() { return ops_; }
HostSharedObjects   &SharedObjectRig::shared() { return objects_; }
SharedObjectContext &SharedObjectRig::first() { return first_; }
SharedObjectContext &SharedObjectRig::second() { return second_; }

bool SharedObjectRig::invariants_hold() const
{
    return objects_.invariants_hold();
}

}  // namespace av
