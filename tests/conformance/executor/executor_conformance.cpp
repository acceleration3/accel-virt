#include "conformance/executor/executor_conformance.hpp"

#include "conformance/executor/executor_transport.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_writer.hpp"

#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

namespace av {

namespace {
struct ApiBit {
    std::uint32_t sub_api;
    std::uint32_t commands;
};

constexpr ApiBit api_bits[] = {{AV_EXECUTOR_SUB_API_VULKAN, 1u},
                               {AV_EXECUTOR_SUB_API_OPENGL, 3u},
                               {AV_EXECUTOR_SUB_API_OPENCL, 2u}};

constexpr std::uint32_t unclaimed_sub_api = 0x40u;

constexpr std::uint8_t no_such_gpu[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

}

ExecutorConformance::ExecutorConformance()
    : gpu_{}, real_gpu_(false), domain_(0u), library_(nullptr), ops_(nullptr), findings_{}, skip_reasons_{},
      found_(0u), skips_(0u), checked_(0u), worked_(0u)
{}

void ExecutorConformance::note(const char *finding)
{
    if (found_ < max_findings) findings_[found_++] = finding;
}

void ExecutorConformance::skipped_because(const char *reason)
{
    if (skips_ < max_findings) skip_reasons_[skips_++] = reason;
}

void ExecutorConformance::require(bool held, const char *finding)
{
    checked_++;
    if (!held) note(finding);
}

std::uint32_t ExecutorConformance::findings() const { return found_; }
const char   *ExecutorConformance::finding(std::uint32_t index) const
{
    return index < found_ ? findings_[index] : nullptr;
}
std::uint32_t ExecutorConformance::checked() const { return checked_; }
std::uint32_t ExecutorConformance::skipped() const { return skips_; }
const char   *ExecutorConformance::skip(std::uint32_t index) const
{
    return index < skips_ ? skip_reasons_[index] : nullptr;
}

bool ExecutorConformance::defined_result(std::uint32_t result) const
{
    return result == AV_EXECUTED || result == AV_UNKNOWN || result == AV_UNDECODABLE ||
           result == AV_DEVICE_LOST;
}

std::uint32_t ExecutorConformance::impossible_id(std::uint32_t api) const
{
    return (api << 16u) | 0xFFFFu;
}

bool ExecutorConformance::use_gpu(const char *hex)
{
    if (hex == nullptr) return false;
    for (std::uint32_t i = 0u; i < sizeof gpu_; i++) {
        std::uint32_t value = 0u;
        for (std::uint32_t nibble = 0u; nibble < 2u; nibble++) {
            const char c = hex[2u * i + nibble];
            std::uint32_t digit = 0u;
            if (c >= '0' && c <= '9') digit = static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') digit = static_cast<std::uint32_t>(c - 'a') + 10u;
            else if (c >= 'A' && c <= 'F') digit = static_cast<std::uint32_t>(c - 'A') + 10u;
            else return false;
            value = (value << 4u) | digit;
        }
        gpu_[i] = static_cast<std::uint8_t>(value);
    }
    real_gpu_ = true;
    return true;
}

bool ExecutorConformance::run(const char *path)
{
    library_ = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (library_ == nullptr) {
        note("the library could not be loaded");
        return false;
    }
    auto *const answer =
        reinterpret_cast<AvExecutorOpsFunction>(dlsym(library_, AV_EXECUTOR_OPS_SYMBOL));
    require(answer != nullptr, "the library exports no av_executor_ops");
    if (answer == nullptr) return false;

    ops_ = answer();
    require(ops_ != nullptr, "av_executor_ops answered no table");
    if (ops_ == nullptr) return false;

    check_table();
    if (found_ != 0u) return false;
    check_sub_apis();
    domain_ = ops_->open_domain(ops_->opaque, real_gpu_ ? gpu_ : no_such_gpu);
    if (domain_ == 0u) skipped_because("the library opened no domain on this GPU; no executor could be made");
    check_memory();
    check_create_and_destroy();
    check_options();
    if (domain_ != 0u) ops_->close_domain(ops_->opaque, domain_);
    return found_ == 0u;
}

void ExecutorConformance::check_table()
{
    require(ops_->abi_version == AV_EXECUTOR_ABI_VERSION, "the table's abi_version is not this device's");
    require(ops_->sub_apis != nullptr && ops_->open_domain != nullptr && ops_->close_domain != nullptr &&
                ops_->allocate_memory != nullptr && ops_->free_memory != nullptr && ops_->create != nullptr &&
                ops_->destroy != nullptr &&
                ops_->execute != nullptr && ops_->end_batch != nullptr && ops_->poll != nullptr &&
                ops_->gpu_progress != nullptr && ops_->set_signal != nullptr && ops_->import_blob != nullptr &&
                ops_->release_blob != nullptr && ops_->imported_handle != nullptr &&
                ops_->share_object != nullptr && ops_->open_object != nullptr && ops_->close_object != nullptr &&
                ops_->release_shared != nullptr && ops_->read_image != nullptr && ops_->export_image != nullptr &&
                ops_->map_frame != nullptr && ops_->unmap_frame != nullptr && ops_->export_surface != nullptr &&
                ops_->sync_surface != nullptr && ops_->answer_part != nullptr,
            "the table is missing an operation");
}

AvContextOptions ExecutorConformance::in_domain(const AvContextOptions &options) const
{
    AvContextOptions placed = options;
    placed.domain = domain_;
    return placed;
}

void ExecutorConformance::check_options()
{
    const std::uint32_t    offered = ops_->sub_apis(ops_->opaque);
    const AvContextOptions plain_options = in_domain(AvContextOptions{});
    const AvContextOptions core_46 = in_domain({4u, 6u, AV_CONTEXT_PROFILE_CORE, 0u, 0u});
    const AvContextOptions gl_50 = in_domain({5u, 0u, AV_CONTEXT_PROFILE_CORE, 0u, 0u});
    require(ops_->create(ops_->opaque, unclaimed_sub_api, &plain_options, nullptr) == nullptr,
            "create answered an executor for a sub-API nobody defined");
    for (const ApiBit &api : api_bits) {
        if ((offered & api.sub_api) == 0u) continue;
        AvExecutor *const plain = ops_->create(ops_->opaque, api.sub_api, &plain_options, nullptr);
        if (plain == nullptr) {
            skipped_because("an offered sub-API made no executor with no options; its options went unchecked");
            continue;
        }
        if (api.sub_api != AV_EXECUTOR_SUB_API_OPENGL) {
            require(ops_->create(ops_->opaque, api.sub_api, &core_46, nullptr) == nullptr,
                    "create made a context with OpenGL options for another sub-API");
            require(ops_->create(ops_->opaque, api.sub_api, &plain_options, plain) == nullptr,
                    "create shared an executor of a sub-API that shares nothing");
        } else if (real_gpu_) {
            AvExecutor *const core = ops_->create(ops_->opaque, api.sub_api, &core_46, nullptr);
            require(core != nullptr, "create refused an OpenGL 4.6 core context on a real GPU");
            AvExecutor *const shared = core != nullptr ? ops_->create(ops_->opaque, api.sub_api, &core_46, core)
                                                       : nullptr;
            require(core == nullptr || shared != nullptr,
                    "create refused an OpenGL context sharing another on its GPU");
            require(ops_->create(ops_->opaque, api.sub_api, &gl_50, nullptr) == nullptr,
                    "create made an OpenGL 5.0 context");
            if (shared != nullptr) ops_->destroy(ops_->opaque, shared);
            if (core != nullptr) ops_->destroy(ops_->opaque, core);
        } else {
            skipped_because("OpenGL's options were not checked without a real GPU");
        }
        ops_->destroy(ops_->opaque, plain);
    }
}

void ExecutorConformance::check_sub_apis()
{
    const std::uint32_t offered = ops_->sub_apis(ops_->opaque);
    require((offered & ~15u) == 0u, "sub_apis offers a bit the protocol does not define");
}

void ExecutorConformance::check_create_and_destroy()
{
    const std::uint32_t    offered = ops_->sub_apis(ops_->opaque);
    const AvContextOptions options = in_domain(AvContextOptions{});
    const AvContextOptions nowhere = {0u, 0u, 0u, 0u, domain_ + 0x1000u};
    require(ops_->create(ops_->opaque, unclaimed_sub_api, &options, nullptr) == nullptr,
            "create answered an executor for a sub-API nobody defined");
    require(ops_->create(ops_->opaque, 0u, &options, nullptr) == nullptr,
            "create answered an executor for no sub-API at all");
    require(ops_->create(ops_->opaque, AV_EXECUTOR_SUB_API_VULKAN, &nowhere, nullptr) == nullptr,
            "create made an executor in a domain no device opened");
    require(ops_->create(ops_->opaque, AV_EXECUTOR_SUB_API_VULKAN, nullptr, nullptr) == nullptr,
            "create made an executor with no options, so in no domain");

    bool made_any = false;
    for (const ApiBit &api : api_bits) {
        if ((offered & api.sub_api) == 0u) {
            require(ops_->create(ops_->opaque, api.sub_api, &options, nullptr) == nullptr,
                    "create answered an executor for a sub-API sub_apis does not offer");
            continue;
        }
        AvExecutor *const executor = ops_->create(ops_->opaque, api.sub_api, &options, nullptr);
        if (executor == nullptr) {
            skipped_because("an offered sub-API made no executor for an unknown GPU; "
                            "execute and imports went unchecked for it");
            continue;
        }
        made_any = true;
        check_execute(executor, api.commands);
        check_workload(executor, api.sub_api);
        check_reply_capacity(executor, api.commands);
        check_memory_channel(executor, api.sub_api);
        if (api.sub_api == AV_EXECUTOR_SUB_API_VULKAN) check_imported_handle(executor);
        ops_->destroy(ops_->opaque, executor);
    }
    if (!made_any && offered != 0u)
        skipped_because("no sub-API this library offers made an executor on this host");
}

void ExecutorConformance::check_execute(AvExecutor *executor, std::uint32_t api)
{
    const std::uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::size_t        produced = 0xDEADu;

    const std::uint32_t no_reply =
        ops_->execute(ops_->opaque, executor, impossible_id(api), payload, sizeof payload, nullptr,
                      0u, &produced);
    require(defined_result(no_reply), "execute answered a result the ABI does not define");
    require(no_reply == AV_UNKNOWN,
            "execute faulted on a command id no profile can carry instead of skipping it");

    produced = 0xDEADu;
    const std::uint32_t empty =
        ops_->execute(ops_->opaque, executor, impossible_id(api), nullptr, 0u, nullptr, 0u,
                      &produced);
    require(defined_result(empty), "execute answered a result the ABI does not define");
}

void ExecutorConformance::check_reply_capacity(AvExecutor *executor, std::uint32_t api)
{
    std::uint8_t reply[64];
    std::memset(reply, 0xAB, sizeof reply);
    const std::size_t capacity = sizeof reply - 1u;
    std::size_t       produced = 0u;
    const std::uint32_t result = ops_->execute(ops_->opaque, executor, impossible_id(api), nullptr,
                                               0u, reply, capacity, &produced);
    require(defined_result(result), "execute answered a result the ABI does not define");
    require(reply[sizeof reply - 1u] == 0xABu, "execute wrote past the reply capacity it was given");
    require(result != AV_EXECUTED || produced >= capacity ||
                reply[produced] == 0xABu,
            "an answer wrote more than the length it reported");
}

void ExecutorConformance::check_workload(AvExecutor *executor, uint32_t sub_api)
{
    std::uint8_t frame[1024] = {};
    std::uint8_t reply[512] = {};
    std::size_t  produced = 0u;
    std::uint32_t command = 0u;
    std::size_t   length = 0u;

    if (sub_api == AV_EXECUTOR_SUB_API_VULKAN) {
        av::WireWriter writer(frame, sizeof frame);
        VkApplicationInfo application = {};
        application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application.pApplicationName = "executor_conformance";
        application.apiVersion = (1u << 22) | (1u << 12);
        VkInstanceCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create.pApplicationInfo = &application;
        av::vkproto::serialize_VkInstanceCreateInfo(writer, create);
        command = av::vkproto::kCmd_vkCreateInstance;
        length = writer.offset();
    } else if (sub_api == AV_EXECUTOR_SUB_API_OPENCL) {
        av::WireWriter writer(frame, sizeof frame);
        writer.write_pod<std::uint32_t>(0u);
        writer.write_pod<std::uint8_t>(0u);
        writer.write_pod<std::uint8_t>(1u);
        command = av::clproto::kCmd_clGetPlatformIDs;
        length = writer.offset();
    } else {
        av::WireWriter writer(frame, sizeof frame);
        writer.write_varint<std::uint32_t>(0x4000u);
        command = av::glproto::kCmd_glClear;
        length = writer.offset();
    }

    const std::uint32_t result =
        ops_->execute(ops_->opaque, executor, command, frame, length, reply, sizeof reply,
                      &produced);
    require(defined_result(result), "a workload answered a result the ABI does not define");
    if (result == AV_UNKNOWN) {
        skipped_because("a sub-API is offered whose simplest command this library does not "
                        "carry: a stub rather than an implementation");
        return;
    }
    require(result == AV_EXECUTED,
            "a library that offers a sub-API could not run that sub-API's simplest command");
    if (result == AV_EXECUTED) worked_++;
}

std::uint32_t ExecutorConformance::allocate(std::uint32_t domain, std::uint64_t key, std::uint64_t size,
                                            HostMemoryRequest request, AvHostMemory &memory)
{
    memory = AvHostMemory{-1, 0u, 0u};
    return ops_->allocate_memory(ops_->opaque, domain, key, size, request.encoded(), &memory);
}

void ExecutorConformance::let_go(std::uint32_t domain, std::uint64_t key, AvHostMemory &memory)
{
    if (memory.fd >= 0) (void)close(memory.fd);
    memory.fd = -1;
    ops_->free_memory(ops_->opaque, domain, key);
}

bool ExecutorConformance::check_usable(const AvHostMemory &memory, std::uint64_t size)
{
    const std::uint32_t before = found_;
    require(memory.fd >= 0, "allocate_memory answered memory with no descriptor");
    require(memory.size >= size, "allocate_memory answered less memory than was asked for");
    require(memory.size % page_bytes == 0u, "allocate_memory answered a size that is not whole host pages");
    require(memory.caching == AV_MEMORY_WRITE_BACK || memory.caching == AV_MEMORY_WRITE_COMBINED,
            "allocate_memory answered a caching the ABI does not define");
    if (memory.fd < 0 || memory.size < size || memory.size == 0u) return false;

    const off_t end = lseek(memory.fd, 0, SEEK_END);
    (void)lseek(memory.fd, 0, SEEK_SET);
    require(end >= 0 && static_cast<std::uint64_t>(end) >= memory.size,
            "the descriptor allocate_memory answered is shorter than the size it answered");

    const std::size_t bytes = static_cast<std::size_t>(memory.size);
    void *const       first = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, memory.fd, 0);
    void *const       second = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, memory.fd, 0);
    require(first != MAP_FAILED && second != MAP_FAILED,
            "the descriptor allocate_memory answered does not map for reading and writing");
    if (first != MAP_FAILED && second != MAP_FAILED) {
        auto *const one = static_cast<volatile std::uint8_t *>(first);
        auto *const other = static_cast<volatile std::uint8_t *>(second);
        bool        seen = true;
        for (std::size_t at = 0u; at < 64u; at++) {
            one[at] = static_cast<std::uint8_t>(at * 13u + 5u);
            other[bytes - 1u - at] = static_cast<std::uint8_t>(at * 29u + 3u);
        }
        for (std::size_t at = 0u; at < 64u; at++) {
            seen = seen && other[at] == static_cast<std::uint8_t>(at * 13u + 5u);
            seen = seen && one[bytes - 1u - at] == static_cast<std::uint8_t>(at * 29u + 3u);
        }
        require(seen, "two mappings of the descriptor allocate_memory answered do not see each other's writes");
    }
    if (first != MAP_FAILED) (void)munmap(first, bytes);
    if (second != MAP_FAILED) (void)munmap(second, bytes);
    return found_ == before;
}

std::uint32_t ExecutorConformance::second_domain(const char *unchecked)
{
    const std::uint32_t other = ops_->open_domain(ops_->opaque, real_gpu_ ? gpu_ : no_such_gpu);
    if (other == 0u) {
        skipped_because(unchecked);
        return 0u;
    }
    require(other != domain_, "open_domain answered a domain that is already open");
    return other != domain_ ? other : 0u;
}

void ExecutorConformance::check_memory()
{
    check_memory_refusals();
    if (domain_ == 0u) {
        skipped_because("no domain on this GPU: no memory was asked for, and the classes went unchecked");
        return;
    }
    check_request_classes();
    check_keys_per_domain();
}

void ExecutorConformance::check_memory_refusals()
{
    const HostMemoryRequest readback(HostMemoryRequest::Kind::readback, 0u);
    const std::uint32_t     nowhere = domain_ + 0x1000u;
    const std::uint32_t     home = domain_ != 0u ? domain_ : nowhere;
    AvHostMemory            memory = {-1, 0u, 0u};
    auto                    refuses = [&](std::uint32_t domain, std::uint64_t size) {
        const bool refused = allocate(domain, refused_key, size, readback, memory) != 0u;
        if (!refused) let_go(domain, refused_key, memory);
        return refused;
    };
    require(refuses(home, 0u), "allocate_memory answered memory of no size");
    require(refuses(home, 1u), "allocate_memory answered one byte, which is not a whole host page");
    require(refuses(home, page_bytes + 1u), "allocate_memory answered a size that is not a whole number of host pages");
    require(refuses(0u, page_bytes), "allocate_memory answered memory in domain 0, which open_domain never answers");
    require(refuses(nowhere, page_bytes), "allocate_memory answered memory in a domain no device opened");

    ops_->free_memory(ops_->opaque, home, never_allocated_key);
    ops_->free_memory(ops_->opaque, nowhere, never_allocated_key);
    ops_->free_memory(ops_->opaque, 0u, never_allocated_key);
    require(true, "free_memory tolerated keys and domains that hold nothing");
}

void ExecutorConformance::check_request_classes()
{
    const HostMemoryRequest classes[] = {HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u),
                                         HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u)};
    for (const HostMemoryRequest &request : classes) {
        AvHostMemory        memory = {-1, 0u, 0u};
        const std::uint32_t answered = allocate(domain_, class_key, page_bytes, request, memory);
        require(answered == 0u || !real_gpu_, "allocate_memory refused a page of upload or readback memory on a "
                                              "real GPU");
        if (answered == 0u) (void)check_usable(memory, page_bytes);
        if (answered == 0u) let_go(domain_, class_key, memory);
    }
    AvHostMemory memory = {-1, 0u, 0u};
    if (allocate(domain_, class_key, 2u * page_bytes, classes[1], memory) == 0u) {
        (void)check_usable(memory, 2u * page_bytes);
        let_go(domain_, class_key, memory);
    }

    for (std::uint32_t type = 0u; type < HostMemoryRequest::memory_types; type++) {
        if (allocate(domain_, class_key, page_bytes, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, type),
                     memory) != 0u)
            continue;
        (void)check_usable(memory, page_bytes);
        let_go(domain_, class_key, memory);
    }

    const std::uint32_t first = allocate(domain_, class_key, page_bytes, classes[1], memory);
    if (first == 0u) let_go(domain_, class_key, memory);
    const std::uint32_t again = allocate(domain_, class_key, page_bytes, classes[1], memory);
    require(first != 0u || again == 0u, "allocate_memory refused a key free_memory had let go of");
    if (again == 0u) let_go(domain_, class_key, memory);
    ops_->free_memory(ops_->opaque, domain_, class_key);
    require(true, "free_memory tolerated a key it had already let go of");
}

void ExecutorConformance::check_keys_per_domain()
{
    const std::uint32_t other = second_domain("a second domain was not opened; keys per domain went unchecked");
    if (other == 0u) return;
    const HostMemoryRequest readback(HostMemoryRequest::Kind::readback, 0u);
    AvHostMemory            mine = {-1, 0u, 0u};
    AvHostMemory            theirs = {-1, 0u, 0u};
    const bool              made_mine = allocate(domain_, domain_key, page_bytes, readback, mine) == 0u;
    const bool              made_theirs = allocate(other, domain_key, page_bytes, readback, theirs) == 0u;
    require(made_mine == made_theirs, "one key was answered in one domain and refused in another");
    if (made_mine && made_theirs && mine.fd >= 0 && theirs.fd >= 0) {
        void *const one = mmap(nullptr, page_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, mine.fd, 0);
        void *const two = mmap(nullptr, page_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, theirs.fd, 0);
        if (one != MAP_FAILED && two != MAP_FAILED) {
            std::memset(two, 0, page_bytes);
            std::memset(one, 0xA5, page_bytes);
            require(static_cast<const std::uint8_t *>(two)[0] == 0u &&
                        static_cast<const std::uint8_t *>(two)[page_bytes - 1u] == 0u,
                    "one key in two domains answered the same memory");
        }
        if (one != MAP_FAILED) (void)munmap(one, page_bytes);
        if (two != MAP_FAILED) (void)munmap(two, page_bytes);
    }
    let_go(domain_, domain_key, mine);
    if (theirs.fd >= 0) (void)close(theirs.fd);
    ops_->close_domain(ops_->opaque, other);
    ops_->free_memory(ops_->opaque, other, domain_key);
    require(true, "free_memory tolerated a key of a domain that was closed");
}

void ExecutorConformance::check_memory_channel(AvExecutor *executor, std::uint32_t sub_api)
{
    ops_->release_blob(ops_->opaque, executor, never_imported_blob);
    require(true, "release_blob tolerated an id that was never imported");
    require(ops_->imported_handle(ops_->opaque, executor, never_imported_blob) == 0u,
            "imported_handle answered a handle for an id that was never imported");
    require(ops_->import_blob(ops_->opaque, executor, channel_blob, never_allocated_key, 0u, 0u) == AV_UNDECODABLE,
            "import_blob took a key no memory was allocated under");
    require(ops_->imported_handle(ops_->opaque, executor, channel_blob) == 0u,
            "imported_handle answered a handle for an import that was refused");
    if (domain_ == 0u) return;
    check_foreign_key(executor, HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u));

    AvHostMemory memory = {-1, 0u, 0u};
    if (allocate(domain_, channel_key, page_bytes, HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u),
                 memory) != 0u) {
        skipped_because("readback memory was refused, so no executor was asked to import any");
        return;
    }
    if (memory.fd >= 0) (void)close(memory.fd);
    const std::uint32_t imported = ops_->import_blob(ops_->opaque, executor, channel_blob, channel_key, 0u, 0u);
    require(imported == AV_EXECUTED || imported == AV_UNDECODABLE,
            "import_blob answered something other than AV_EXECUTED or AV_UNDECODABLE");
    if (imported == AV_EXECUTED) {
        const std::uint64_t handle = ops_->imported_handle(ops_->opaque, executor, channel_blob);
        check_other_executor(executor, sub_api, channel_blob, handle);
        ops_->free_memory(ops_->opaque, domain_, channel_key);
        require(ops_->imported_handle(ops_->opaque, executor, channel_blob) == handle,
                "free_memory took an import's memory from under it");
        ops_->release_blob(ops_->opaque, executor, channel_blob);
        require(ops_->imported_handle(ops_->opaque, executor, channel_blob) == 0u,
                "imported_handle still answered a handle after the blob was released");
        ops_->release_blob(ops_->opaque, executor, channel_blob);
        require(true, "release_blob tolerated an id it had already released");
    } else {
        require(ops_->imported_handle(ops_->opaque, executor, channel_blob) == 0u,
                "imported_handle answered a handle for an import that was refused");
        ops_->free_memory(ops_->opaque, domain_, channel_key);
    }
    const std::uint32_t late = ops_->import_blob(ops_->opaque, executor, channel_blob, channel_key, 0u, 0u);
    require(late == AV_UNDECODABLE, "import_blob took a key after free_memory had let it go");
    if (late == AV_EXECUTED) ops_->release_blob(ops_->opaque, executor, channel_blob);
}

void ExecutorConformance::check_foreign_key(AvExecutor *executor, HostMemoryRequest request)
{
    const std::uint32_t other =
        second_domain("a second domain was not opened; an import across domains went unchecked");
    if (other == 0u) return;
    AvHostMemory memory = {-1, 0u, 0u};
    if (allocate(other, foreign_key, page_bytes, request, memory) == 0u) {
        if (memory.fd >= 0) (void)close(memory.fd);
        const std::uint32_t imported = ops_->import_blob(ops_->opaque, executor, foreign_blob, foreign_key, 0u, 0u);
        require(imported == AV_UNDECODABLE, "import_blob took memory allocated in another domain");
        if (imported == AV_EXECUTED) ops_->release_blob(ops_->opaque, executor, foreign_blob);
        ops_->free_memory(ops_->opaque, other, foreign_key);
    } else {
        skipped_because("a second domain allocated nothing; an import across domains went unchecked");
    }
    ops_->close_domain(ops_->opaque, other);
}

void ExecutorConformance::check_other_executor(AvExecutor *executor, std::uint32_t sub_api, std::uint64_t blob,
                                               std::uint64_t handle)
{
    const AvContextOptions options = in_domain(AvContextOptions{});
    AvExecutor *const      other = ops_->create(ops_->opaque, sub_api, &options, nullptr);
    if (other == nullptr) {
        skipped_because("a second executor was not made; an import's owner went unchecked");
        return;
    }
    require(ops_->imported_handle(ops_->opaque, other, blob) == 0u,
            "imported_handle answered another executor's import");
    ops_->release_blob(ops_->opaque, other, blob);
    require(ops_->imported_handle(ops_->opaque, executor, blob) == handle,
            "release_blob on one executor released another's import");
    ops_->destroy(ops_->opaque, other);
}

bool ExecutorConformance::make_device(ExecutorTransport &wire, std::uint64_t &instance, std::uint64_t &physical,
                                      std::uint64_t &device)
{
    instance = physical = device = 0u;
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "executor_conformance";
    application.apiVersion = (1u << 22) | (1u << 12);
    VkInstanceCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create.pApplicationInfo = &application;
    if (vkproto::av_vkCreateInstance(wire, &create, nullptr, &instance) != VK_SUCCESS) instance = 0u;
    if (instance == 0u) return false;

    std::uint32_t count = 1u;
    if (vkproto::av_vkEnumeratePhysicalDevices(wire, as<VkInstance>(instance), &count, &physical) != VK_SUCCESS ||
        count == 0u)
        physical = 0u;
    if (physical == 0u) return false;

    float                   priority = 1.0f;
    VkDeviceQueueCreateInfo queue = {};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueCount = 1u;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo device_create = {};
    device_create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create.queueCreateInfoCount = 1u;
    device_create.pQueueCreateInfos = &queue;
    if (vkproto::av_vkCreateDevice(wire, as<VkPhysicalDevice>(physical), &device_create, nullptr, &device) !=
        VK_SUCCESS)
        device = 0u;
    return device != 0u;
}

std::uint64_t ExecutorConformance::make_buffer(ExecutorTransport &wire, std::uint64_t device)
{
    VkBufferCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create.size = page_bytes;
    create.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkproto::av_vkCreateBuffer(wire, as<VkDevice>(device), &create, nullptr, &buffer) != VK_SUCCESS)
        return 0u;
    return reinterpret_cast<std::uint64_t>(buffer);
}

void ExecutorConformance::check_vulkan_types(const VkPhysicalDeviceMemoryProperties &properties)
{
    std::uint32_t visible = 0u;
    for (std::uint32_t type = 0u; type < properties.memoryTypeCount && type < VK_MAX_MEMORY_TYPES; type++) {
        const VkMemoryPropertyFlags flags = properties.memoryTypes[type].propertyFlags;
        if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0u) continue;
        visible++;
        AvHostMemory        memory = {-1, 0u, 0u};
        const std::uint32_t answered =
            allocate(domain_, type_key, page_bytes, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, type), memory);
        require(answered == 0u, "allocate_memory refused a Vulkan type the guest's device shows host-visible");
        if (answered != 0u) continue;
        (void)check_usable(memory, page_bytes);
        require((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0u || memory.caching == AV_MEMORY_WRITE_COMBINED,
                "memory of a Vulkan type that is not host-cached was answered write-back");
        let_go(domain_, type_key, memory);
    }
    require(visible != 0u, "the guest's device shows no host-visible memory type");

    AvHostMemory memory = {-1, 0u, 0u};
    if (properties.memoryTypeCount < HostMemoryRequest::memory_types) {
        require(allocate(domain_, type_key, page_bytes,
                         HostMemoryRequest(HostMemoryRequest::Kind::vulkan, properties.memoryTypeCount),
                         memory) != 0u,
                "allocate_memory answered a Vulkan memory type the GPU does not have");
        let_go(domain_, type_key, memory);
    }
    std::uint64_t heaps = 0u;
    for (std::uint32_t heap = 0u; heap < properties.memoryHeapCount && heap < VK_MAX_MEMORY_HEAPS; heap++)
        heaps += properties.memoryHeaps[heap].size;
    const std::uint64_t too_much = (2u * heaps + page_bytes) / page_bytes * page_bytes;
    require(allocate(domain_, type_key, too_much, HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u), memory) !=
                0u,
            "allocate_memory answered more memory than every heap of the GPU holds");
    let_go(domain_, type_key, memory);
}

void ExecutorConformance::check_imported_handle(AvExecutor *executor)
{
    require(ops_->imported_handle(ops_->opaque, executor, imported_blob) == 0u,
            "imported_handle answered a handle for an id that was never imported");
    if (domain_ == 0u) return;
    ExecutorTransport wire(*ops_, executor);
    std::uint64_t     instance = 0u;
    std::uint64_t     physical = 0u;
    std::uint64_t     device = 0u;
    if (!make_device(wire, instance, physical, device)) {
        skipped_because("a Vulkan executor made no device; an imported handle went unchecked");
        if (instance != 0u) vkproto::av_vkDestroyInstance(wire, as<VkInstance>(instance), nullptr);
        return;
    }
    VkPhysicalDeviceMemoryProperties properties = {};
    vkproto::av_vkGetPhysicalDeviceMemoryProperties(wire, as<VkPhysicalDevice>(physical), &properties);
    require(wire.ran(), "a Vulkan executor with a device did not answer its memory properties");
    if (wire.ran()) check_vulkan_types(properties);

    const std::uint64_t buffer = make_buffer(wire, device);
    require(buffer != 0u, "a Vulkan executor with a device made no buffer");
    VkMemoryRequirements requirements = {};
    if (buffer != 0u)
        vkproto::av_vkGetBufferMemoryRequirements(wire, as<VkDevice>(device), as<VkBuffer>(buffer), &requirements);
    std::uint32_t mappable = 0u;
    for (std::uint32_t type = 0u; type < properties.memoryTypeCount && type < VK_MAX_MEMORY_TYPES; type++)
        if ((properties.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u)
            mappable |= 1u << type;
    const std::uint32_t types = requirements.size <= page_bytes ? requirements.memoryTypeBits & mappable : 0u;
    require(buffer == 0u || types != 0u, "no host-visible memory type takes a one-page buffer");
    if (types != 0u)
        check_gpu_round_trip(executor, wire, device, buffer, static_cast<std::uint32_t>(__builtin_ctz(types)));

    if (buffer != 0u) vkproto::av_vkDestroyBuffer(wire, as<VkDevice>(device), as<VkBuffer>(buffer), nullptr);
    vkproto::av_vkDestroyDevice(wire, as<VkDevice>(device), nullptr);
    vkproto::av_vkDestroyInstance(wire, as<VkInstance>(instance), nullptr);
}

void ExecutorConformance::check_gpu_round_trip(AvExecutor *executor, ExecutorTransport &wire, std::uint64_t device,
                                               std::uint64_t buffer, std::uint32_t type)
{
    constexpr std::uint64_t half = page_bytes / 2u;
    AvHostMemory            memory = {-1, 0u, 0u};
    const HostMemoryRequest request(HostMemoryRequest::Kind::vulkan, type);
    if (allocate(domain_, imported_key, page_bytes, request, memory) != 0u || memory.fd < 0) {
        let_go(domain_, imported_key, memory);
        return;
    }
    void *const mapped = mmap(nullptr, page_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, memory.fd, 0);
    auto *const window = mapped != MAP_FAILED ? static_cast<volatile std::uint8_t *>(mapped) : nullptr;
    if (window != nullptr)
        for (std::uint64_t at = 0u; at < page_bytes; at++)
            window[at] = static_cast<std::uint8_t>(at < half ? at * 7u + 1u : 0u);

    const std::uint32_t imported = ops_->import_blob(ops_->opaque, executor, imported_blob, imported_key, 0u, 0u);
    require(imported == AV_EXECUTED,
            "import_blob refused memory allocated for a type the guest's device maps, once it had a device");
    const std::uint64_t handle = ops_->imported_handle(ops_->opaque, executor, imported_blob);
    require(imported != AV_EXECUTED || handle != 0u, "imported_handle answered no handle for an import it took");
    check_foreign_key(executor, request);

    const VkDevice vk_device = as<VkDevice>(device);
    const VkBuffer vk_buffer = as<VkBuffer>(buffer);
    const bool     bound = handle != 0u && vkproto::av_vkBindBufferMemory(wire, vk_device, vk_buffer,
                                                                       as<VkDeviceMemory>(handle), 0u) == VK_SUCCESS;
    require(handle == 0u || bound, "execute did not accept an imported handle back in vkBindBufferMemory");

    std::uint64_t queue = 0u;
    VkCommandPool pool = VK_NULL_HANDLE;
    if (bound) vkproto::av_vkGetDeviceQueue(wire, vk_device, 0u, 0u, &queue);
    VkCommandPoolCreateInfo pool_create = {};
    pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    if (queue != 0u && vkproto::av_vkCreateCommandPool(wire, vk_device, &pool_create, nullptr, &pool) != VK_SUCCESS)
        pool = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 2u;
    std::uint64_t commands[2] = {};
    const bool    recordable = pool != VK_NULL_HANDLE &&
                            vkproto::av_vkAllocateCommandBuffers(wire, vk_device, &allocate_info, commands) ==
                                VK_SUCCESS &&
                            commands[0] != 0u && commands[1] != 0u;
    require(!bound || recordable, "a Vulkan executor with a device made no command buffers");

    VkMemoryBarrier to_host = {};
    to_host.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    to_host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    const VkQueue vk_queue = as<VkQueue>(queue);
    auto          submit = [&](std::uint32_t which) {
        const VkCommandBuffer recorded = as<VkCommandBuffer>(commands[which]);
        VkSubmitInfo          info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.commandBufferCount = 1u;
        info.pCommandBuffers = &recorded;
        return vkproto::av_vkEndCommandBuffer(wire, recorded) == VK_SUCCESS &&
               vkproto::av_vkQueueSubmit(wire, vk_queue, 1u, &info, VK_NULL_HANDLE) == VK_SUCCESS &&
               vkproto::av_vkQueueWaitIdle(wire, vk_queue) == VK_SUCCESS;
    };

    if (recordable && window != nullptr) {
        const VkCommandBuffer copying = as<VkCommandBuffer>(commands[0]);
        const VkBufferCopy    region = {0u, half, half};
        bool                  ran = vkproto::av_vkBeginCommandBuffer(wire, copying, &begin) == VK_SUCCESS;
        vkproto::av_vkCmdCopyBuffer(wire, copying, vk_buffer, vk_buffer, 1u, &region);
        ran = ran && wire.ran();
        vkproto::av_vkCmdPipelineBarrier(wire, copying, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                         0u, 1u, &to_host, 0u, nullptr, 0u, nullptr);
        ran = ran && wire.ran() && submit(0u);
        require(ran, "a copy within a buffer bound to imported memory did not run");
        bool copied = ran;
        for (std::uint64_t at = 0u; at < half && copied; at++)
            copied = window[half + at] == static_cast<std::uint8_t>(at * 7u + 1u);
        require(!ran || copied, "a GPU copy through imported memory did not carry what the CPU wrote through its "
                                "descriptor, or the CPU did not see what the GPU wrote");

        ops_->free_memory(ops_->opaque, domain_, imported_key);
        require(ops_->imported_handle(ops_->opaque, executor, imported_blob) == handle,
                "free_memory took an import's memory from under it");
        const VkCommandBuffer filling = as<VkCommandBuffer>(commands[1]);
        ran = vkproto::av_vkBeginCommandBuffer(wire, filling, &begin) == VK_SUCCESS;
        vkproto::av_vkCmdFillBuffer(wire, filling, vk_buffer, half, half, 0x5A5A5A5Au);
        ran = ran && wire.ran();
        vkproto::av_vkCmdPipelineBarrier(wire, filling, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                         0u, 1u, &to_host, 0u, nullptr, 0u, nullptr);
        ran = ran && wire.ran() && submit(1u);
        require(ran, "a fill of imported memory did not run once the device had let go of its key");
        bool filled = ran;
        for (std::uint64_t at = half; at < page_bytes && filled; at++) filled = window[at] == 0x5Au;
        require(!ran || filled, "once the device let go of the key, the import was no longer the memory its "
                                "descriptor maps");
    }
    if (pool != VK_NULL_HANDLE) vkproto::av_vkDestroyCommandPool(wire, vk_device, pool, nullptr);

    if (imported == AV_EXECUTED) {
        check_other_executor(executor, AV_EXECUTOR_SUB_API_VULKAN, imported_blob, handle);
        ops_->release_blob(ops_->opaque, executor, imported_blob);
        require(ops_->imported_handle(ops_->opaque, executor, imported_blob) == 0u,
                "imported_handle still answered a handle after the blob was released");
        ops_->release_blob(ops_->opaque, executor, imported_blob);
        require(true, "release_blob tolerated an id it had already released");
    }
    if (mapped != MAP_FAILED) (void)munmap(mapped, page_bytes);
    let_go(domain_, imported_key, memory);
}

}
