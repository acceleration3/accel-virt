#include "harness/executor/import_rig.hpp"

#include "device/control/control_request.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/context/context_resources.hpp"
#include "proto/context/sub_api.hpp"

#include <cstdlib>
#include <cstring>

namespace av {

namespace {

constexpr std::uint8_t no_uuid[ContextExecutor::uuid_bytes] = {};
/* Where RESOURCE_CREATE_BLOB's memory entries would start, after a header. */
constexpr std::size_t   entries_at = 24u;
constexpr std::size_t   entry_size = 16u;

}  // namespace

/* A rig that cannot make its events, thread or lock has nothing to test with. */
ImportRig::ImportRig(bool executor)
    : host_(), ops_(RVK_EXECUTOR_SUB_API_VULKAN), library_(), executor_(host_, library_), blobs_(host_),
      imports_(host_, blobs_, executor_), start_(nullptr), done_(nullptr), thread_(nullptr), job_(nullptr),
      job_context_(nullptr), stopping_(false)
{
    host_.add_guest_ram(ram, ram_pages * page);
    for (std::uint32_t index = 0u; index < ram_pages; index++)
        std::memset(guest_page(index), static_cast<int>((0x10u + index) & 0xFFu), page);
    if (blobs_.initialize() != Status::ok || library_.accept(&ops_.table()) != Status::ok ||
        host_.create_event(start_) != Status::ok || host_.create_event(done_) != Status::ok ||
        host_.start_thread([](void *rig) { static_cast<ImportRig *>(rig)->serve(); }, this, thread_) != Status::ok)
        std::abort();
    executor_.configure(SubApi::vulkan, no_uuid);
    imports_.configure(ring);
    if (executor) on_worker([this] { static_cast<void>(executor_.create()); });
}

ImportRig::~ImportRig()
{
    on_worker([this] { executor_.destroy(); });
    stopping_ = true;
    host_.signal_event(start_);
    host_.join_thread(thread_);
    host_.destroy_event(done_);
    host_.destroy_event(start_);
}

void ImportRig::run_job(void (*job)(void *), void *context)
{
    job_ = job;
    job_context_ = context;
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

MockHost        &ImportRig::host() { return host_; }
MockExecutorOps &ImportRig::ops() { return ops_; }
BlobTable       &ImportRig::blobs() { return blobs_; }
BlobImports     &ImportRig::imports() { return imports_; }
ContextExecutor &ImportRig::executor() { return executor_; }

std::uint8_t *ImportRig::guest_page(std::uint32_t index)
{
    return host_.guest_ram(ram + index * page);
}

std::uint32_t ImportRig::chunk(std::uint32_t slot) const
{
    return ContextResources(ring).chunk(slot);
}

Status ImportRig::create_guest_blob(std::uint32_t id, const std::uint32_t *pages, std::uint32_t count)
{
    const std::size_t   size = entries_at + entry_size * count;
    std::uint8_t *const bytes = static_cast<std::uint8_t *>(std::calloc(size, 1u));
    for (std::uint32_t index = 0u; index < count; index++) {
        const std::uint64_t address = ram + pages[index] * page;
        const std::uint32_t length = static_cast<std::uint32_t>(page);
        std::memcpy(bytes + entries_at + index * entry_size, &address, 8u);
        std::memcpy(bytes + entries_at + index * entry_size + 8u, &length, 4u);
    }
    MockRequest    mock(bytes, size);
    ControlRequest control(host_, &mock);
    const Status   created = blobs_.create_guest(id, count * page, control, entries_at, count);
    std::free(bytes);
    return created;
}

}  // namespace av
