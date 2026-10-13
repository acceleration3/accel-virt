// An executor library for the qtest suites (src/device/abi/av_executor.h): it
// offers the Vulkan sub-API and runs a handful of commands, so the device's
// execution path can be driven on the real device without render-server.
//
//   0x10001 echo         the payload is the answer
//   0x10002 undecodable  AV_UNDECODABLE
//   0x10003 lose device  AV_DEVICE_LOST
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
//   0x10007 crash        with AV_ECHO_ALLOW_CRASH set, aborts the process the
//                        executor runs in, as a driver crash would; otherwise
//                        AV_UNKNOWN
//
//   0x30007 options      on an OpenGL context: the answer is the u8 major,
//                        minor, profile and flags it was created with, then
//                        the u64 ordinal of the executor it shares (0: none)
//
// Anything else is AV_UNKNOWN. With AV_ECHO_FAIL_CREATE set, create fails.
//
// Version 15 (docs/external-export.md): every surface the guest has not
// released is shared, so the device's export socket can be judged without a
// GPU. export_surface answers a memfd of 64x64 B8G8R8A8 pixels, linear, whose
// first eight bytes are the surface's id; sync_surface answers twice the id as
// the copies run, and an eventfd already readable - signalled - standing in
// for a sync_file. release_shared remembers what the guest let go of (0
// forgets it all: the ids start again), and a surface let go of exports
// nothing.
//
// create records the options it was asked for (feature 050). With
// AV_ECHO_OPENGL set the library offers OpenGL beside Vulkan.
//
// import_blob records a host pointer the device hands it (feature 045),
// imported_handle answers blob_id | 0x4000000000000000 for one it holds, and
// release_blob forgets it - so qtest can judge the device's side of
// IMPORT_BLOB, reading and writing the guest's pages through the pointer,
// without a GPU.
//
// Domains are counted and nothing else: a device's domain is the one the last
// open_domain answered.
//
// Two controls make a real timeout happen without rebuilding anything
// (feature 040, FR-006), read from the environment at load:
//
//   AV_ECHO_STALL_MS      every command sleeps this long before answering
//   AV_ECHO_FAULT_AFTER   the Nth command on each executor answers
//                          AV_DEVICE_LOST, whatever it asked for
//   AV_ECHO_ONLY_NTH      both apply only to the Nth executor created, so one
//                          context can stall or fault while another completes
//
// A stall longer than TdrDelay is what dxgkrnl times out on; the guest suite's
// tdr scenario sets them for the run that needs them.
#include "av_executor.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {

constexpr std::uint32_t echo = 0x10001u;
constexpr std::uint32_t undecodable = 0x10002u;
constexpr std::uint32_t lose_device = 0x10003u;
constexpr std::uint32_t crash = 0x10007u;
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
     * what AV_ECHO_ONLY_NTH names. */
    std::uint64_t ordinal;
    bool          one_thread;
    Import        imports[max_imports];
    AvContextOptions options;
    /* The ordinal of the executor it shares objects with, or 0. */
    std::uint64_t     shares;
};

Executor *executor_of(AvExecutor *handle) { return static_cast<Executor *>(static_cast<void *>(handle)); }

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
/* Domains opened: the main loop opens them, any worker reads the count. */
std::uint32_t domains = 0u;

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
    const std::uint32_t opengl = std::getenv("AV_ECHO_OPENGL") != nullptr ? AV_EXECUTOR_SUB_API_OPENGL : 0u;
    return AV_EXECUTOR_SUB_API_VULKAN | opengl;
}

AvExecutor *make(void *opaque, std::uint32_t sub_api)
{
    /* Only what sub_apis offers. Accepting anything looks harmless in a test
     * library and is not: the device would hand that context commands of an
     * API nothing here decodes, and the guest would wait for answers that
     * never come. The conformance suite found this one (feature 042). */
    if ((sub_apis(opaque) & sub_api) == 0u || sub_api == 0u || (sub_api & (sub_api - 1u)) != 0u) return nullptr;
    if (std::getenv("AV_ECHO_FAIL_CREATE") != nullptr) return nullptr;
    auto *const executor = static_cast<Executor *>(std::calloc(1u, sizeof(Executor)));
    if (executor == nullptr) return nullptr;
    executor->thread = pthread_self();
    executor->ordinal = ++created;
    executor->one_thread = true;
    return static_cast<AvExecutor *>(static_cast<void *>(executor));
}

bool defaults(const AvContextOptions *options)
{
    return options == nullptr || (options->major == 0u && options->minor == 0u && options->profile == 0u &&
                                  options->flags == 0u);
}

/* Options and a share only for OpenGL, and a share only of an OpenGL executor;
 * in a domain this library opened. */
AvExecutor *create(void *opaque, std::uint32_t sub_api, const AvContextOptions *options, AvExecutor *share)
{
    const bool opengl = sub_api == AV_EXECUTOR_SUB_API_OPENGL;
    if (options == nullptr || options->domain == 0u || options->domain > __atomic_load_n(&domains, __ATOMIC_ACQUIRE)) return nullptr;
    if ((!defaults(options) || share != nullptr) && !opengl) return nullptr;
    if (options->major > 4u) return nullptr;
    AvExecutor *const made = make(opaque, sub_api);
    if (made == nullptr) return nullptr;
    Executor *const executor = executor_of(made);
    executor->options = *options;
    executor->shares = share != nullptr ? executor_of(share)->ordinal : 0u;
    return made;
}

void destroy(void *, AvExecutor *handle) { std::free(handle); }

std::uint32_t execute(void *, AvExecutor *handle, std::uint32_t id, const std::uint8_t *payload, std::size_t size,
                      std::uint8_t *reply, std::size_t capacity, std::size_t *reply_size)
{
    auto *const executor = static_cast<Executor *>(static_cast<void *>(handle));
    executor->one_thread = executor->one_thread && pthread_equal(executor->thread, pthread_self()) != 0;
    *reply_size = 0u;
    /* The host taking too long, and the host losing the device: what a guest
     * cannot ask for and a test has to be able to produce. Either is aimed at
     * one executor where the run named one, so that what happens to a context
     * can be told apart from what happens to the device. */
    const std::uint64_t only = from_environment("AV_ECHO_ONLY_NTH");
    const bool steered = only == 0u || only == executor->ordinal;
    if (steered) stall(from_environment("AV_ECHO_STALL_MS"));
    const std::uint64_t fault_after = from_environment("AV_ECHO_FAULT_AFTER");
    if (steered && fault_after != 0u && executor->executed + 1u >= fault_after) return AV_DEVICE_LOST;
    switch (id) {
    case echo:
        *reply_size = size;
        if (size <= capacity && size != 0u) std::memcpy(reply, payload, size);
        break;
    case undecodable: return AV_UNDECODABLE;
    case lose_device: return AV_DEVICE_LOST;
    case crash:
        if (std::getenv("AV_ECHO_ALLOW_CRASH") == nullptr) return AV_UNKNOWN;
        std::abort();
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
    default: return AV_UNKNOWN;
    }
    executor->executed++;
    return AV_EXECUTED;
}

/* Copy at the boundary: the capset the echo executor's device describes stays
 * what it was before the library imported anything. IMPORT_BLOB does not ask. */
std::uint32_t import_kinds(void *, std::uint32_t, const std::uint8_t *) { return 0u; }

/* A host pointer of whole pages and nothing else: a dma-buf's fd would be the
 * callee's to close, and nothing here would use it. */
std::uint32_t import_blob(void *, AvExecutor *handle, std::uint64_t blob_id, const AvBlobHandle *blob)
{
    Executor *const executor = executor_of(handle);
    if (blob == nullptr || blob->kind != AV_BLOB_HOST_POINTER || blob->pointer == nullptr || blob->size == 0u ||
        blob->size % page != 0u)
        return AV_UNDECODABLE;
    for (Import &import : executor->imports) {
        if (import.live) continue;
        import = Import{blob_id, static_cast<std::uint8_t *>(blob->pointer), blob->size, blob->reserved, true};
        return AV_EXECUTED;
    }
    return AV_UNDECODABLE;
}

void release_blob(void *, AvExecutor *handle, std::uint64_t blob_id)
{
    for (Import &import : executor_of(handle)->imports)
        if (import.live && import.blob_id == blob_id) import.live = false;
}

std::uint64_t imported_handle(void *, AvExecutor *handle, std::uint64_t blob_id)
{
    return import_named(executor_of(handle), blob_id | handle_bit) != nullptr ? blob_id | handle_bit : 0u;
}

/* An echo executor completes everything on the thread that asked, so there is
 * nothing to signal about: it takes the callback and never calls it, which is
 * what the device must be able to cope with. */
void set_signal(void *, AvExecutor *, void (*)(void *), void *) {}

std::uint32_t open_domain(void *, const std::uint8_t *) { return __atomic_add_fetch(&domains, 1u, __ATOMIC_ACQ_REL); }
void          close_domain(void *, std::uint32_t) {}

void          end_batch(void *, AvExecutor *) {}
std::uint32_t poll(void *, AvExecutor *) { return 0u; }
void          gpu_progress(void *, AvExecutor *, std::uint64_t *marked, std::uint64_t *reached)
{
    *marked = 0u;
    *reached = 0u;
}

/* An echo library has no host API behind it, so it makes no host object and
 * shares none. The device answers every shared-object command with "the
 * executor refused", which is a status and not a fault, and that refusal is
 * what the qemu suites see. */
std::uint32_t share_object(void *, AvExecutor *, std::uint64_t, std::uint32_t, std::uint64_t, std::uint64_t)
{
    return AV_SHARED_REFUSED;
}
std::uint32_t open_object(void *, AvExecutor *, std::uint64_t, std::uint32_t, std::uint64_t, std::uint64_t *)
{
    return AV_SHARED_REFUSED;
}
void close_object(void *, AvExecutor *, std::uint64_t) {}

/* Nothing is drawn here, so there is no image to read or export and no frame
 * staged. */
std::uint32_t read_image(void *, std::uint32_t, std::uint64_t, void *, std::uint32_t, std::uint32_t, std::uint32_t)
{
    return AV_UNKNOWN;
}
std::uint32_t export_image(void *, std::uint32_t, std::uint64_t, AvExportedImage *) { return AV_UNKNOWN; }
std::uint32_t map_frame(void *, std::uint32_t, std::uint64_t, std::uint32_t, std::uint32_t, AvFrameView *)
{
    return AV_UNKNOWN;
}
void unmap_frame(void *, std::uint32_t, std::uint64_t, std::uint32_t) {}

/* Shared surfaces (docs/external-export.md): what the guest let go of, under a lock, since
 * the export socket asks from a thread of its own and the release comes on
 * the main loop. */
constexpr std::uint32_t surface_edge = 64u;
constexpr std::size_t   max_released = 64u;
pthread_mutex_t         released_lock = PTHREAD_MUTEX_INITIALIZER;
std::uint64_t           released[max_released] = {};

bool surface_released(std::uint64_t surface)
{
    pthread_mutex_lock(&released_lock);
    bool found = surface == 0u;
    for (const std::uint64_t gone : released) found = found || gone == surface;
    pthread_mutex_unlock(&released_lock);
    return found;
}

void release_shared(void *, std::uint32_t, std::uint64_t object_id)
{
    pthread_mutex_lock(&released_lock);
    if (object_id == 0u) std::memset(released, 0, sizeof released);
    for (std::uint64_t &slot : released) {
        if (object_id == 0u || slot != 0u) continue;
        slot = object_id;
        break;
    }
    pthread_mutex_unlock(&released_lock);
}

std::uint32_t export_surface(void *, std::uint32_t, std::uint64_t surface, AvExportedImage *image)
{
    constexpr std::uint32_t stride = surface_edge * 4u;
    constexpr std::size_t   bytes = std::size_t{stride} * surface_edge;
    if (image == nullptr || surface_released(surface)) return AV_UNKNOWN;
    const int fd = memfd_create("echo-surface", MFD_CLOEXEC);
    if (fd < 0) return AV_UNKNOWN;
    void *mapped = MAP_FAILED;
    if (ftruncate(fd, static_cast<off_t>(bytes)) == 0)
        mapped = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        close(fd);
        return AV_UNKNOWN;
    }
    std::memcpy(mapped, &surface, sizeof surface);
    munmap(mapped, bytes);
    *image = AvExportedImage{fd, 0u, surface_edge, surface_edge, stride, 0u, 0x34325241u /* AR24 */, 0u, 0u};
    return AV_EXECUTED;
}

std::uint32_t sync_surface(void *, std::uint32_t, std::uint64_t surface, std::uint64_t *writes, int *fd)
{
    if (writes == nullptr || fd == nullptr || surface_released(surface)) return AV_UNKNOWN;
    *fd = eventfd(1u, EFD_CLOEXEC);
    if (*fd < 0) return AV_UNKNOWN;
    *writes = surface * 2u;
    return AV_EXECUTED;
}

const AvExecutorOps table = {AV_EXECUTOR_ABI_VERSION,
                              nullptr,
                              sub_apis,
                              import_kinds,
                              open_domain,
                              close_domain,
                              create,
                              destroy,
                              execute,
                              end_batch,
                              poll,
                              gpu_progress,
                              set_signal,
                              import_blob,
                              release_blob,
                              imported_handle,
                              share_object,
                              open_object,
                              close_object,
                              release_shared,
                              read_image,
                              export_image,
                              map_frame,
                              unmap_frame,
                              export_surface,
                              sync_surface,
                              nullptr,
                              nullptr};

}  // namespace

extern "C" {

__attribute__((visibility("default")))
const AvExecutorOps *av_executor_ops(void)
{
    return &table;
}

}
