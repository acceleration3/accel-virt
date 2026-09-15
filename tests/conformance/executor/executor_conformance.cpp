#include "conformance/executor/executor_conformance.hpp"

#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>

namespace av {

namespace {

/* Every sub-API bit the ABI defines, and the command-API number each one's
 * command ids carry in their top 16 bits. OpenGL is capset bit 2 and command
 * API 3, which is a remap worth reading twice. */
struct ApiBit {
    std::uint32_t sub_api;
    std::uint32_t commands;
};

constexpr ApiBit api_bits[] = {{RVK_EXECUTOR_SUB_API_VULKAN, 1u},
                               {RVK_EXECUTOR_SUB_API_OPENGL, 3u},
                               {RVK_EXECUTOR_SUB_API_OPENCL, 2u}};

/* A bit no sub-API has, which no library may accept in create. */
constexpr std::uint32_t unclaimed_sub_api = 0x40u;

constexpr std::uint8_t no_such_gpu[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

}  // namespace

ExecutorConformance::ExecutorConformance()
    : gpu_{}, real_gpu_(false), library_(nullptr), ops_(nullptr), findings_{}, skip_reasons_{},
      found_(0u), skips_(0u),
      checked_(0u), worked_(0u), frame_{}, reply_{}, produced_(0u)
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
    return result == RVK_EXECUTED || result == RVK_UNKNOWN || result == RVK_UNDECODABLE ||
           result == RVK_DEVICE_LOST;
}

std::uint32_t ExecutorConformance::impossible_id(std::uint32_t api) const
{
    /* Inside the API's range, so the device would deliver it, and past
     * anything a registry could number. */
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
        reinterpret_cast<RvkExecutorOpsFunction>(dlsym(library_, RVK_EXECUTOR_OPS_SYMBOL));
    require(answer != nullptr, "the library exports no rvk_executor_ops");
    if (answer == nullptr) return false;

    ops_ = answer();
    require(ops_ != nullptr, "rvk_executor_ops answered no table");
    if (ops_ == nullptr) return false;

    check_table();
    if (found_ != 0u) return false;
    check_sub_apis();
    check_create_and_destroy();
    return found_ == 0u;
}

void ExecutorConformance::check_table()
{
    require(ops_->abi_version >= RVK_EXECUTOR_ABI_VERSION_MINIMUM &&
                ops_->abi_version <= RVK_EXECUTOR_ABI_VERSION,
            "the table's abi_version is one this device does not know");
    require(ops_->sub_apis != nullptr && ops_->create != nullptr && ops_->destroy != nullptr &&
                ops_->execute != nullptr,
            "the table is missing an operation version 1 requires");
    if (ops_->abi_version < memory_channel_version) {
        require(true, "a version 1 table needs no memory channel");
        return;
    }
    /* All three or none: a table claiming the version and carrying half of it
     * is found out one blob later, on a worker, with a guest waiting. */
    require(ops_->import_kinds != nullptr && ops_->import_blob != nullptr &&
                ops_->release_blob != nullptr,
            "the table claims version 2 and is missing one of its memory operations");
    if (ops_->abi_version < imported_handle_version) return;
    require(ops_->imported_handle != nullptr, "the table claims version 3 and has no imported_handle");
}

void ExecutorConformance::check_sub_apis()
{
    /* Asked first, before anything else has happened, because the device asks
     * it on its main loop during realize with no GPU context in hand. */
    const std::uint32_t offered = ops_->sub_apis(ops_->opaque);
    require((offered & ~7u) == 0u, "sub_apis offers a bit the protocol does not define");

    if (ops_->abi_version < memory_channel_version) return;
    for (const ApiBit &api : api_bits) {
        const std::uint32_t kinds =
            ops_->import_kinds(ops_->opaque, api.sub_api, real_gpu_ ? gpu_ : no_such_gpu);
        require((kinds & ~3u) == 0u, "import_kinds offers a blob kind the ABI does not define");
        require((offered & api.sub_api) != 0u || kinds == 0u,
                "import_kinds offers a kind for a sub-API sub_apis does not");
    }
}

void ExecutorConformance::check_create_and_destroy()
{
    const std::uint32_t offered = ops_->sub_apis(ops_->opaque);
    require(ops_->create(ops_->opaque, unclaimed_sub_api, no_such_gpu) == nullptr,
            "create answered an executor for a sub-API nobody defined");
    require(ops_->create(ops_->opaque, 0u, no_such_gpu) == nullptr,
            "create answered an executor for no sub-API at all");

    bool made_any = false;
    for (const ApiBit &api : api_bits) {
        if ((offered & api.sub_api) == 0u) {
            require(ops_->create(ops_->opaque, api.sub_api, no_such_gpu) == nullptr,
                    "create answered an executor for a sub-API sub_apis does not offer");
            continue;
        }
        RvkExecutor *const executor =
            ops_->create(ops_->opaque, api.sub_api, real_gpu_ ? gpu_ : no_such_gpu);
        if (executor == nullptr) {
            /* Refusing a GPU nobody has is correct for a library that binds
             * one, so this is not a finding - but it does mean the statements
             * below went unchecked for this sub-API. */
            skipped_because("an offered sub-API made no executor for an unknown GPU; "
                            "execute and the memory channel went unchecked for it");
            continue;
        }
        made_any = true;
        check_execute(executor, api.commands);
        check_workload(executor, api.sub_api);
        check_reply_capacity(executor, api.commands);
        if (ops_->abi_version >= memory_channel_version) check_memory_channel(executor);
        if (ops_->abi_version >= imported_handle_version && api.sub_api == RVK_EXECUTOR_SUB_API_VULKAN)
            check_imported_handle(executor);
        ops_->destroy(ops_->opaque, executor);
    }
    if (!made_any && offered != 0u)
        skipped_because("no sub-API this library offers made an executor on this host");
}

void ExecutorConformance::check_execute(RvkExecutor *executor, std::uint32_t api)
{
    const std::uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::size_t        produced = 0xDEADu;

    /* Every command without the reply flag is handed a null reply and a
     * capacity of zero, which must not be a failure. */
    const std::uint32_t no_reply =
        ops_->execute(ops_->opaque, executor, impossible_id(api), payload, sizeof payload, nullptr,
                      0u, &produced);
    require(defined_result(no_reply), "execute answered a result the ABI does not define");
    require(no_reply == RVK_UNKNOWN,
            "execute faulted on a command id no profile can carry instead of skipping it");

    /* An empty payload is a frame a guest can send; it must decide, not crash. */
    produced = 0xDEADu;
    const std::uint32_t empty =
        ops_->execute(ops_->opaque, executor, impossible_id(api), nullptr, 0u, nullptr, 0u,
                      &produced);
    require(defined_result(empty), "execute answered a result the ABI does not define");
}

void ExecutorConformance::check_reply_capacity(RvkExecutor *executor, std::uint32_t api)
{
    /* A guard byte past the capacity: an answer that does not fit writes
     * nothing, and one that does must not write past what it was given. */
    std::uint8_t reply[64];
    std::memset(reply, 0xAB, sizeof reply);
    const std::size_t capacity = sizeof reply - 1u;
    std::size_t       produced = 0u;
    const std::uint32_t result = ops_->execute(ops_->opaque, executor, impossible_id(api), nullptr,
                                               0u, reply, capacity, &produced);
    require(defined_result(result), "execute answered a result the ABI does not define");
    require(reply[sizeof reply - 1u] == 0xABu, "execute wrote past the reply capacity it was given");
    /* An answer shorter than the capacity must not have written past what it
     * says it wrote; a longer one may leave a prefix, which the device
     * discards. */
    require(result != RVK_EXECUTED || produced >= capacity ||
                reply[produced] == 0xABu,
            "an answer wrote more than the length it reported");
}

void ExecutorConformance::check_workload(RvkExecutor *executor, uint32_t sub_api)
{
    std::uint8_t frame[1024] = {};
    std::uint8_t reply[512] = {};
    std::size_t  produced = 0u;
    std::uint32_t command = 0u;
    std::size_t   length = 0u;

    if (sub_api == RVK_EXECUTOR_SUB_API_VULKAN) {
        /* The first command of every Vulkan program there has ever been. */
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
    } else if (sub_api == RVK_EXECUTOR_SUB_API_OPENCL) {
        av::WireWriter writer(frame, sizeof frame);
        writer.write_pod<std::uint32_t>(0u);
        writer.write_pod<std::uint8_t>(0u);
        writer.write_pod<std::uint8_t>(1u);
        command = av::clproto::kCmd_clGetPlatformIDs;
        length = writer.offset();
    } else {
        /* An OpenGL clear, which needs a context that is current and a driver
         * that answers - and asks for nothing back. OpenGL's wire carries
         * scalars as varints: written as a raw u32 this decoded, from its first
         * byte, as glClear(0), and passed for the wrong reason. */
        av::WireWriter writer(frame, sizeof frame);
        writer.write_varint<std::uint32_t>(0x4000u);
        command = av::glproto::kCmd_glClear;
        length = writer.offset();
    }

    const std::uint32_t result =
        ops_->execute(ops_->opaque, executor, command, frame, length, reply, sizeof reply,
                      &produced);
    require(defined_result(result), "a workload answered a result the ABI does not define");
    if (result == RVK_UNKNOWN) {
        /* A library that offers a sub-API and carries none of its commands is
         * a stub rather than a wrong implementation - the echo library is
         * exactly that, on purpose - and the device treats it as one: every
         * command skipped, no fault. Worth saying, not worth failing. */
        skipped_because("a sub-API is offered whose simplest command this library does not "
                        "carry: a stub rather than an implementation");
        return;
    }
    require(result == RVK_EXECUTED,
            "a library that offers a sub-API could not run that sub-API's simplest command");
    if (result == RVK_EXECUTED) worked_++;
}

void ExecutorConformance::check_memory_channel(RvkExecutor *executor)
{
    /* Release is idempotent: the device may release an id whose import failed,
     * and a library that faults on one is a library that hangs a teardown. */
    ops_->release_blob(ops_->opaque, executor, 0xFFFFFFFFu);
    require(true, "release_blob tolerated an id that was never imported");

    RvkBlobHandle refused = {};
    refused.kind = 0u;
    refused.fd = -1;
    refused.size = 0u;
    require(ops_->import_blob(ops_->opaque, executor, 1u, &refused) == RVK_UNDECODABLE,
            "import_blob accepted a handle of no kind and no size");

    RvkBlobHandle two_kinds = {};
    two_kinds.kind = RVK_BLOB_HOST_POINTER | RVK_BLOB_DMA_BUF;
    two_kinds.fd = -1;
    two_kinds.size = 4096u;
    require(ops_->import_blob(ops_->opaque, executor, 2u, &two_kinds) == RVK_UNDECODABLE,
            "import_blob accepted a handle that is two kinds at once");

    RvkBlobHandle short_page = {};
    short_page.kind = RVK_BLOB_HOST_POINTER;
    short_page.pointer = this;
    short_page.fd = -1;
    short_page.size = 1u;
    require(ops_->import_blob(ops_->opaque, executor, 3u, &short_page) == RVK_UNDECODABLE,
            "import_blob accepted a size that is not a whole number of pages");
}

std::uint32_t ExecutorConformance::send(RvkExecutor *executor, std::uint32_t command, std::size_t length)
{
    produced_ = 0u;
    return ops_->execute(ops_->opaque, executor, command, frame_, length, reply_, sizeof reply_, &produced_);
}

std::int32_t ExecutorConformance::answered_result() const
{
    av::WireReader read(reply_, produced_ < sizeof reply_ ? produced_ : 0u);
    std::int32_t   returned = -1;
    return read.read_pod(returned) ? returned : -1;
}

std::uint64_t ExecutorConformance::answered_handle() const
{
    av::WireReader read(reply_, produced_ < sizeof reply_ ? produced_ : 0u);
    std::int32_t   returned = -1;
    std::uint64_t  handle = 0u;
    if (!read.read_pod(returned) || !read.read_pod(handle)) return 0u;
    return returned == VK_SUCCESS ? handle : 0u;
}

bool ExecutorConformance::make_device(RvkExecutor *executor, std::uint64_t &instance, std::uint64_t &device)
{
    av::WireWriter    writer(frame_, sizeof frame_);
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.apiVersion = (1u << 22) | (1u << 12);
    VkInstanceCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create.pApplicationInfo = &application;
    av::vkproto::serialize_VkInstanceCreateInfo(writer, create);
    instance = send(executor, av::vkproto::kCmd_vkCreateInstance, writer.offset()) == RVK_EXECUTED
                   ? answered_handle()
                   : 0u;
    if (instance == 0u) return false;

    av::WireWriter listing(frame_, sizeof frame_);
    listing.write_pod(instance);
    listing.write_pod<std::uint8_t>(1u);
    listing.write_pod<std::uint32_t>(1u);
    if (send(executor, av::vkproto::kCmd_vkEnumeratePhysicalDevices, listing.offset()) != RVK_EXECUTED) return false;
    av::WireReader listed(reply_, produced_ < sizeof reply_ ? produced_ : 0u);
    std::int32_t   returned = -1;
    std::uint32_t  count = 0u;
    std::uint64_t  physical = 0u;
    if (!listed.read_pod(returned) || !listed.read_pod(count) || !listed.read_pod(physical) || count == 0u)
        return false;

    /* The guest names no extension: making the device able to import is the
     * executor's part, not the guest's. */
    av::WireWriter          making(frame_, sizeof frame_);
    float                   priority = 1.0f;
    VkDeviceQueueCreateInfo queue = {};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueCount = 1u;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo device_create = {};
    device_create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create.queueCreateInfoCount = 1u;
    device_create.pQueueCreateInfos = &queue;
    making.write_pod(physical);
    av::vkproto::serialize_VkDeviceCreateInfo(making, device_create);
    device = send(executor, av::vkproto::kCmd_vkCreateDevice, making.offset()) == RVK_EXECUTED ? answered_handle()
                                                                                                : 0u;
    return device != 0u;
}

std::uint64_t ExecutorConformance::make_buffer(RvkExecutor *executor, std::uint64_t device)
{
    av::WireWriter                   writer(frame_, sizeof frame_);
    VkExternalMemoryBufferCreateInfo external = {};
    external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    VkBufferCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create.pNext = &external;
    create.size = page_bytes;
    create.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    writer.write_pod(device);
    av::vkproto::serialize_VkBufferCreateInfo(writer, create);
    return send(executor, av::vkproto::kCmd_vkCreateBuffer, writer.offset()) == RVK_EXECUTED ? answered_handle() : 0u;
}

std::uint32_t ExecutorConformance::buffer_types(RvkExecutor *executor, std::uint64_t device, std::uint64_t buffer)
{
    av::WireWriter writer(frame_, sizeof frame_);
    writer.write_pod(device);
    writer.write_pod(buffer);
    if (send(executor, av::vkproto::kCmd_vkGetBufferMemoryRequirements, writer.offset()) != RVK_EXECUTED) return 0u;
    std::uint8_t         block[256];
    av::WireArena        arena(block, sizeof block);
    av::WireReader       read(reply_, produced_ < sizeof reply_ ? produced_ : 0u);
    std::int32_t         returned = -1;
    VkMemoryRequirements requirements = {};
    if (!read.read_pod(returned) || !av::vkproto::deserialize_VkMemoryRequirements(read, &requirements, arena))
        return 0u;
    return requirements.size <= page_bytes ? requirements.memoryTypeBits : 0u;
}

std::uint64_t ExecutorConformance::import_page(RvkExecutor *executor, void *page, std::uint32_t types)
{
    for (std::uint32_t type = 0u; type < VK_MAX_MEMORY_TYPES; type++) {
        if (((types >> type) & 1u) == 0u) continue;
        RvkBlobHandle handle = {};
        handle.kind = RVK_BLOB_HOST_POINTER;
        handle.reserved = type;
        handle.pointer = page;
        handle.fd = -1;
        handle.size = page_bytes;
        /* A type the executor will not import this pointer into is refused
         * and the id stays free, so the next type is asked under the same id. */
        if (ops_->import_blob(ops_->opaque, executor, imported_blob, &handle) == RVK_EXECUTED)
            return ops_->imported_handle(ops_->opaque, executor, imported_blob);
    }
    return 0u;
}

void ExecutorConformance::destroy_handle(RvkExecutor *executor, std::uint32_t command, std::uint64_t parent,
                                         std::uint64_t handle)
{
    av::WireWriter writer(frame_, sizeof frame_);
    if (parent != 0u) writer.write_pod(parent);
    writer.write_pod(handle);
    (void)send(executor, command, writer.offset());
}

void ExecutorConformance::check_imported_handle(RvkExecutor *executor)
{
    require(ops_->imported_handle(ops_->opaque, executor, imported_blob) == 0u,
            "imported_handle answered a handle for an id that was never imported");
    std::uint64_t instance = 0u;
    std::uint64_t device = 0u;
    if (!make_device(executor, instance, device)) {
        skipped_because("a Vulkan executor made no device; an imported handle went unchecked");
        if (instance != 0u) destroy_handle(executor, av::vkproto::kCmd_vkDestroyInstance, 0u, instance);
        return;
    }
    const std::uint64_t buffer = make_buffer(executor, device);
    require(buffer != 0u, "a Vulkan executor with a device made no buffer");
    void *const page = mmap(nullptr, page_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    const std::uint64_t memory =
        buffer != 0u ? import_page(executor, page, buffer_types(executor, device, buffer)) : 0u;
    require(memory != 0u, "no memory type a buffer accepts took an import of a host page with a handle");
    if (memory != 0u) {
        av::WireWriter writer(frame_, sizeof frame_);
        writer.write_pod(device);
        writer.write_pod(buffer);
        writer.write_pod(memory);
        writer.write_pod<std::uint64_t>(0u);
        const std::uint32_t bound = send(executor, av::vkproto::kCmd_vkBindBufferMemory, writer.offset());
        require(bound == RVK_EXECUTED && answered_result() == VK_SUCCESS,
                "execute did not accept an imported handle back in vkBindBufferMemory");
        destroy_handle(executor, av::vkproto::kCmd_vkDestroyBuffer, device, buffer);
        ops_->release_blob(ops_->opaque, executor, imported_blob);
        require(ops_->imported_handle(ops_->opaque, executor, imported_blob) == 0u,
                "imported_handle still answered a handle after the blob was released");
    }
    destroy_handle(executor, av::vkproto::kCmd_vkDestroyDevice, 0u, device);
    destroy_handle(executor, av::vkproto::kCmd_vkDestroyInstance, 0u, instance);
    (void)munmap(page, page_bytes);
}

}  // namespace av
