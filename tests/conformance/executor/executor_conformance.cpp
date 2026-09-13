#include "conformance/executor/executor_conformance.hpp"

#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstring>
#include <dlfcn.h>

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
      checked_(0u), worked_(0u)
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
    if (ops_->abi_version < RVK_EXECUTOR_ABI_VERSION) {
        require(true, "a version 1 table needs no memory channel");
        return;
    }
    /* All three or none: a table claiming the version and carrying half of it
     * is found out one blob later, on a worker, with a guest waiting. */
    require(ops_->import_kinds != nullptr && ops_->import_blob != nullptr &&
                ops_->release_blob != nullptr,
            "the table claims version 2 and is missing one of its memory operations");
}

void ExecutorConformance::check_sub_apis()
{
    /* Asked first, before anything else has happened, because the device asks
     * it on its main loop during realize with no GPU context in hand. */
    const std::uint32_t offered = ops_->sub_apis(ops_->opaque);
    require((offered & ~7u) == 0u, "sub_apis offers a bit the protocol does not define");

    if (ops_->abi_version < RVK_EXECUTOR_ABI_VERSION) return;
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
        if (ops_->abi_version >= RVK_EXECUTOR_ABI_VERSION) check_memory_channel(executor);
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
         * that answers - and asks for nothing back. */
        av::WireWriter writer(frame, sizeof frame);
        writer.write_pod<std::uint32_t>(0x4000u);
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

}  // namespace av
