// An executor library for the qtest suites (src/device/abi/rvk_executor.h): it
// offers the Vulkan sub-API and runs a handful of commands, so the device's
// execution path can be driven on the real device without render-server.
//
//   0x10001 echo         the payload is the answer
//   0x10002 undecodable  RVK_UNDECODABLE
//   0x10003 lose device  RVK_DEVICE_LOST
//   0x10004 count        the answer is u64 commands this executor ran before
//                        it, then u32 1 when every call so far came from the
//                        thread that created it
//   0x10005 peek         payload u64 handle, u64 offset, u32 count: the answer
//                        is the import's u32 memory type, u32 imports this
//                        executor holds, u64 size, then `count` bytes read at
//                        `offset` of the pointer it was handed - only the 16
//                        bytes, zeroed, for a handle it does not hold or a range
//                        past the import
//   0x10006 poke         payload u64 handle, u64 offset, then bytes written at
//                        `offset` of the import's pointer, as a host GPU would
//                        write them: the answer is u32 1 when they were written
//
//   0x30007 options      on an OpenGL context: the answer is the u8 major,
//                        minor, profile and flags it was created with, then
//                        the u64 ordinal of the executor it shares (0: none)
//
// Anything else is RVK_UNKNOWN. With RVK_ECHO_FAIL_CREATE set, create fails.
//
// Version 4 (feature 050): create_with_options records what it was asked for.
// With RVK_ECHO_OPENGL set the library offers OpenGL beside Vulkan, and with
// RVK_ECHO_ABI_VERSION_3 it answers the version 3 table, which makes only
// default contexts.
//
// It is a version 3 library (feature 045): import_blob records a host pointer
// the device hands it, imported_handle answers blob_id | 0x4000000000000000 for
// one it holds, and release_blob forgets it - so qtest can judge the device's
// side of IMPORT_BLOB, reading and writing the guest's pages through the
// pointer, without a GPU. With RVK_ECHO_ABI_VERSION_1 set it answers the
// version 1 table instead, a library that imports nothing.
//
// Two controls make a real timeout happen without rebuilding anything
// (feature 040, FR-006), read from the environment at load:
//
//   RVK_ECHO_STALL_MS      every command sleeps this long before answering
//   RVK_ECHO_FAULT_AFTER   the Nth command on each executor answers
//                          RVK_DEVICE_LOST, whatever it asked for
//   RVK_ECHO_ONLY_NTH      both apply only to the Nth executor created, so one
//                          context can stall or fault while another completes
//
// A stall longer than TdrDelay is what dxgkrnl times out on; the guest suite's
// tdr scenario sets them for the run that needs them.
#include "rvk_executor.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>

namespace {

constexpr std::uint32_t echo = 0x10001u;
constexpr std::uint32_t undecodable = 0x10002u;
constexpr std::uint32_t lose_device = 0x10003u;
constexpr std::uint32_t count = 0x10004u;
constexpr std::uint32_t peek = 0x10005u;
constexpr std::uint32_t poke = 0x10006u;
constexpr std::uint32_t options_asked = 0x30007u;
constexpr std::uint64_t handle_bit = 0x4000000000000000u;
constexpr std::uint64_t page = 4096u;
constexpr std::size_t   max_imports = 16u;

struct Import {
    std::uint64_t  blob_id;
    std::uint8_t  *pointer;
    std::uint64_t  size;
    std::uint32_t  memory_type;
    bool           live;
};

struct Executor {
    pthread_t     thread;
    std::uint64_t executed;
    /* Which executor this is, counted from one in the order they were created:
     * what RVK_ECHO_ONLY_NTH names. */
    std::uint64_t ordinal;
    bool          one_thread;
    Import        imports[max_imports];
    RvkContextOptions options;
    /* The ordinal of the executor it shares objects with, or 0. */
    std::uint64_t     shares;
};

Executor *executor_of(RvkExecutor *handle) { return static_cast<Executor *>(static_cast<void *>(handle)); }

Import *import_named(Executor *executor, std::uint64_t handle)
{
    for (Import &import : executor->imports)
        if (import.live && (import.blob_id | handle_bit) == handle) return &import;
    return nullptr;
}

std::uint32_t live_imports(const Executor *executor)
{
    std::uint32_t live = 0u;
    for (const Import &import : executor->imports) live += import.live ? 1u : 0u;
    return live;
}

/* The import `handle` names and the `length` bytes at `offset` of it, or null
 * for a range it does not hold. */
std::uint8_t *range_of(Executor *executor, const std::uint8_t *payload, std::size_t size, std::size_t length,
                       Import *&import)
{
    std::uint64_t handle = 0u;
    std::uint64_t offset = 0u;
    if (size < 16u) return nullptr;
    std::memcpy(&handle, payload, 8u);
    std::memcpy(&offset, payload + 8u, 8u);
    import = import_named(executor, handle);
    if (import == nullptr || offset > import->size || length > import->size - offset) return nullptr;
    return import->pointer + offset;
}

void answer_peek(Executor *executor, const std::uint8_t *payload, std::size_t size, std::uint8_t *reply,
                 std::size_t capacity, std::size_t *reply_size)
{
    std::uint32_t wanted = 0u;
    if (size >= 20u) std::memcpy(&wanted, payload + 16u, 4u);
    Import *import = nullptr;
    const std::uint8_t *const bytes = range_of(executor, payload, size, wanted, import);
    *reply_size = 16u + (bytes != nullptr ? wanted : 0u);
    if (capacity < *reply_size) return;
    std::memset(reply, 0, 16u);
    if (import == nullptr) return;
    const std::uint32_t live = live_imports(executor);
    std::memcpy(reply, &import->memory_type, 4u);
    std::memcpy(reply + 4u, &live, 4u);
    std::memcpy(reply + 8u, &import->size, 8u);
    if (bytes != nullptr && wanted != 0u) std::memcpy(reply + 16u, bytes, wanted);
}

void answer_poke(Executor *executor, const std::uint8_t *payload, std::size_t size, std::uint8_t *reply,
                 std::size_t capacity, std::size_t *reply_size)
{
    Import *import = nullptr;
    const std::size_t          length = size > 16u ? size - 16u : 0u;
    std::uint8_t *const        bytes = range_of(executor, payload, size, length, import);
    const std::uint32_t        written = bytes != nullptr ? 1u : 0u;
    if (bytes != nullptr && length != 0u) std::memcpy(bytes, payload + 16u, length);
    *reply_size = 4u;
    if (capacity >= 4u) std::memcpy(reply, &written, 4u);
}

std::uint64_t created = 0u;

/* A number the environment names, or 0. */
std::uint64_t from_environment(const char *name)
{
    const char *const value = std::getenv(name);
    if (value == nullptr) return 0u;
    std::uint64_t number = 0u;
    for (const char *digit = value; *digit >= '0' && *digit <= '9'; digit++)
        number = number * 10u + static_cast<std::uint64_t>(*digit - '0');
    return number;
}

void stall(std::uint64_t milliseconds)
{
    if (milliseconds == 0u) return;
    const timespec sleep = {static_cast<time_t>(milliseconds / 1000u),
                            static_cast<long>((milliseconds % 1000u) * 1000000u)};
    static_cast<void>(nanosleep(&sleep, nullptr));
}

std::uint32_t sub_apis(void *)
{
    const std::uint32_t opengl = std::getenv("RVK_ECHO_OPENGL") != nullptr ? RVK_EXECUTOR_SUB_API_OPENGL : 0u;
    return RVK_EXECUTOR_SUB_API_VULKAN | opengl;
}

RvkExecutor *create(void *opaque, std::uint32_t sub_api, const std::uint8_t *)
{
    /* Only what sub_apis offers. Accepting anything looks harmless in a test
     * library and is not: the device would hand that context commands of an
     * API nothing here decodes, and the guest would wait for answers that
     * never come. The conformance suite found this one (feature 042). */
    if ((sub_apis(opaque) & sub_api) == 0u || sub_api == 0u || (sub_api & (sub_api - 1u)) != 0u) return nullptr;
    if (std::getenv("RVK_ECHO_FAIL_CREATE") != nullptr) return nullptr;
    auto *const executor = static_cast<Executor *>(std::calloc(1u, sizeof(Executor)));
    if (executor == nullptr) return nullptr;
    executor->thread = pthread_self();
    executor->ordinal = ++created;
    executor->one_thread = true;
    return static_cast<RvkExecutor *>(static_cast<void *>(executor));
}

bool defaults(const RvkContextOptions *options)
{
    return options == nullptr || (options->major == 0u && options->minor == 0u && options->profile == 0u &&
                                  options->flags == 0u && options->reserved == 0u);
}

/* Options and a share only for OpenGL, and a share only of an OpenGL executor. */
RvkExecutor *create_with_options(void *opaque, std::uint32_t sub_api, const std::uint8_t *uuid,
                                 const RvkContextOptions *options, RvkExecutor *share)
{
    const bool opengl = sub_api == RVK_EXECUTOR_SUB_API_OPENGL;
    if ((!defaults(options) || share != nullptr) && !opengl) return nullptr;
    if (options != nullptr && (options->major > 4u || options->reserved != 0u)) return nullptr;
    RvkExecutor *const made = create(opaque, sub_api, uuid);
    if (made == nullptr) return nullptr;
    Executor *const executor = executor_of(made);
    if (options != nullptr) executor->options = *options;
    executor->shares = share != nullptr ? executor_of(share)->ordinal : 0u;
    return made;
}

void destroy(void *, RvkExecutor *handle) { std::free(handle); }

std::uint32_t execute(void *, RvkExecutor *handle, std::uint32_t id, const std::uint8_t *payload, std::size_t size,
                      std::uint8_t *reply, std::size_t capacity, std::size_t *reply_size)
{
    auto *const executor = static_cast<Executor *>(static_cast<void *>(handle));
    executor->one_thread = executor->one_thread && pthread_equal(executor->thread, pthread_self()) != 0;
    *reply_size = 0u;
    /* The host taking too long, and the host losing the device: what a guest
     * cannot ask for and a test has to be able to produce. Either is aimed at
     * one executor where the run named one, so that what happens to a context
     * can be told apart from what happens to the device. */
    const std::uint64_t only = from_environment("RVK_ECHO_ONLY_NTH");
    const bool steered = only == 0u || only == executor->ordinal;
    if (steered) stall(from_environment("RVK_ECHO_STALL_MS"));
    const std::uint64_t fault_after = from_environment("RVK_ECHO_FAULT_AFTER");
    if (steered && fault_after != 0u && executor->executed + 1u >= fault_after) return RVK_DEVICE_LOST;
    switch (id) {
    case echo:
        *reply_size = size;
        if (size <= capacity && size != 0u) std::memcpy(reply, payload, size);
        break;
    case undecodable: return RVK_UNDECODABLE;
    case lose_device: return RVK_DEVICE_LOST;
    case peek: answer_peek(executor, payload, size, reply, capacity, reply_size); break;
    case poke: answer_poke(executor, payload, size, reply, capacity, reply_size); break;
    case options_asked: {
        *reply_size = 12u;
        if (capacity < 12u) break;
        reply[0] = executor->options.major;
        reply[1] = executor->options.minor;
        reply[2] = executor->options.profile;
        reply[3] = executor->options.flags;
        std::memcpy(reply + 4u, &executor->shares, 8u);
        break;
    }
    case count: {
        const std::uint32_t flag = executor->one_thread ? 1u : 0u;
        *reply_size = 12u;
        if (capacity >= 12u) {
            std::memcpy(reply, &executor->executed, 8u);
            std::memcpy(reply + 8, &flag, 4u);
        }
        break;
    }
    default: return RVK_UNKNOWN;
    }
    executor->executed++;
    return RVK_EXECUTED;
}

/* Copy at the boundary: the capset the echo executor's device describes stays
 * what it was before the library imported anything. IMPORT_BLOB does not ask. */
std::uint32_t import_kinds(void *, std::uint32_t, const std::uint8_t *) { return 0u; }

/* A host pointer of whole pages and nothing else: a dma-buf's fd would be the
 * callee's to close, and nothing here would use it. */
std::uint32_t import_blob(void *, RvkExecutor *handle, std::uint64_t blob_id, const RvkBlobHandle *blob)
{
    Executor *const executor = executor_of(handle);
    if (blob == nullptr || blob->kind != RVK_BLOB_HOST_POINTER || blob->pointer == nullptr || blob->size == 0u ||
        blob->size % page != 0u)
        return RVK_UNDECODABLE;
    for (Import &import : executor->imports) {
        if (import.live) continue;
        import = Import{blob_id, static_cast<std::uint8_t *>(blob->pointer), blob->size, blob->reserved, true};
        return RVK_EXECUTED;
    }
    return RVK_UNDECODABLE;
}

void release_blob(void *, RvkExecutor *handle, std::uint64_t blob_id)
{
    for (Import &import : executor_of(handle)->imports)
        if (import.live && import.blob_id == blob_id) import.live = false;
}

std::uint64_t imported_handle(void *, RvkExecutor *handle, std::uint64_t blob_id)
{
    return import_named(executor_of(handle), blob_id | handle_bit) != nullptr ? blob_id | handle_bit : 0u;
}

/* An echo executor completes everything on the thread that asked, so there is
 * nothing to signal about: it takes the callback and never calls it, which is
 * what the device must be able to cope with. */
void set_signal(void *, RvkExecutor *, void (*)(void *), void *) {}

/* Version 6: an echo library has no host API behind it, so it makes no host
 * object and exports no descriptor of one. The device answers every
 * shared-object command with "the executor refused", which is a status and not
 * a fault, and that refusal is what the qemu suites see. */
uint32_t export_object(void *, RvkExecutor *, uint32_t, uint64_t, uint32_t *, int *)
{
    return RVK_UNKNOWN;
}
uint32_t import_object(void *, RvkExecutor *, uint32_t, uint32_t, int, uint64_t, uint64_t *)
{
    return RVK_UNKNOWN;
}
void release_object(void *, RvkExecutor *, uint32_t, uint64_t) {}

const RvkExecutorOps table = {RVK_EXECUTOR_ABI_VERSION,
                              nullptr,
                              sub_apis,
                              create,
                              destroy,
                              execute,
                              import_kinds,
                              import_blob,
                              release_blob,
                              imported_handle,
                              create_with_options,
                              set_signal,
                              export_object,
                              import_object,
                              release_object, nullptr, nullptr};

/* Version 3: default contexts only (feature 050). */
const RvkExecutorOps version_3_table = {RVK_EXECUTOR_ABI_VERSION_HANDLES,
                                        nullptr,
                                        sub_apis,
                                        create,
                                        destroy,
                                        execute,
                                        import_kinds,
                                        import_blob,
                                        release_blob,
                                        imported_handle,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr, nullptr, nullptr};

/* Version 1, carrying none of the memory operations: the device must keep
 * accepting a library that imports nothing, and a suite whose only library is
 * the newest version never proves it (feature 042). */
const RvkExecutorOps version_1_table = {RVK_EXECUTOR_ABI_VERSION_MINIMUM,
                                        nullptr,
                                        sub_apis,
                                        create,
                                        destroy,
                                        execute,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr, nullptr, nullptr};

}  // namespace

extern "C" {

__attribute__((visibility("default")))
const RvkExecutorOps *rvk_executor_ops(void)
{
    if (std::getenv("RVK_ECHO_ABI_VERSION_1") != nullptr) return &version_1_table;
    return std::getenv("RVK_ECHO_ABI_VERSION_3") != nullptr ? &version_3_table : &table;
}

}
