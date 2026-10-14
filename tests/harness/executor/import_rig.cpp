#include "harness/executor/import_rig.hpp"

#include "proto/context/gl_context_options.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/host_blob_id.hpp"

#include <cstdlib>

namespace av {

namespace {

constexpr std::uint8_t no_uuid[ContextExecutor::uuid_bytes] = {};

}  // namespace

/* A rig that cannot make its events, thread or lock has nothing to test with. */
ImportRig::ImportRig(bool executor)
    : host_(), ops_(AV_EXECUTOR_SUB_API_VULKAN), recorder_(host_), library_(), shares_(host_, library_),
      executor_(host_, library_, shares_, context), plane_(), host_blobs_(host_),
      imports_(host_, host_blobs_, executor_, recorder_, context), start_(nullptr), done_(nullptr),
      thread_(nullptr), job_(nullptr), job_context_(nullptr), stopping_(false)
{
    plane_.allocate_through(library_);
    host_.set_hostmem_size(64u * page);
    if (library_.accept(&ops_.table()) != Status::ok || library_.open_domain(no_uuid) != Status::ok ||
        host_blobs_.initialize(plane_) != Status::ok || host_.create_event(start_) != Status::ok ||
        host_.create_event(done_) != Status::ok ||
        host_.start_thread([](void *rig) { static_cast<ImportRig *>(rig)->serve(); }, this, thread_) != Status::ok)
        std::abort();
    executor_.configure(SubApi::vulkan, no_uuid, GlContextOptions());
    if (executor) on_worker([this] { static_cast<void>(executor_.create()); });
}

/* The blobs go before the domain does, as a device's reset frees them. */
ImportRig::~ImportRig()
{
    on_worker([this] { executor_.destroy(); });
    host_blobs_.release_all();
    stopping_ = true;
    host_.signal_event(start_);
    host_.join_thread(thread_);
    host_.destroy_event(done_);
    host_.destroy_event(start_);
}

void ImportRig::run_job(void (*job)(void *), void *argument)
{
    job_ = job;
    job_context_ = argument;
    host_.signal_event(start_);
    host_.wait_event(done_);
}

void ImportRig::serve()
{
    for (host_.wait_event(start_); !stopping_; host_.wait_event(start_)) {
        job_(job_context_);
        host_.signal_event(done_);
    }
}

MockHost              &ImportRig::host() { return host_; }
MockExecutorOps       &ImportRig::ops() { return ops_; }
HostBlobTable         &ImportRig::host_blobs() { return host_blobs_; }
RecordingContextPlane &ImportRig::plane() { return plane_; }
BlobImports           &ImportRig::imports() { return imports_; }
WireRecorder          &ImportRig::recorder() { return recorder_; }
ContextExecutor       &ImportRig::executor() { return executor_; }

std::uint32_t ImportRig::blob(std::uint32_t slot) const
{
    return HostBlobId::first + slot;
}

Status ImportRig::create_host_blob(std::uint32_t id, std::uint32_t owner, std::uint32_t pages)
{
    return host_blobs_.create(id, owner, HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u), pages * page);
}

std::uint8_t *ImportRig::memory_of(std::uint32_t id)
{
    return ops_.memory_bytes(ops_.last_opened_domain(), id);
}

}  // namespace av
