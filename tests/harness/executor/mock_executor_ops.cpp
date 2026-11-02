#include "harness/executor/mock_executor_ops.hpp"

#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace av {

namespace {

MockExecutorOps &mock_of(void *opaque) { return *static_cast<MockExecutorOps *>(opaque); }

class Lock {
public:
    explicit Lock(pthread_mutex_t &mutex) : mutex_(mutex) { pthread_mutex_lock(&mutex_); }
    ~Lock() { pthread_mutex_unlock(&mutex_); }
    Lock(const Lock &) = delete;
    Lock &operator=(const Lock &) = delete;

private:
    pthread_mutex_t &mutex_;
};

}  // namespace

MockExecutorOps::MockExecutorOps(std::uint32_t sub_apis)
    : table_{}, sub_apis_(sub_apis), imported_(0u), released_blobs_(0u), lock_(), released_(),
      held_(false), waiting_(0u), executors_{}, fail_create_(0u), created_(0u), destroyed_(0u), executed_(0u), kept_(nullptr), kept_length_(0u), ids_{},
      sizes_{}, last_sub_api_(0u), misuses_(0u), imports_{}, refuse_import_(0u), withhold_handle_(0u),
      answered_handle_(0u), option_creates_(0u), signals_set_(0u), signal_(nullptr), signal_context_(nullptr),
      last_options_{}, last_share_(nullptr), last_share_live_(false), creates_held_(false), creates_waiting_(0u)
{
    pthread_mutex_init(&lock_, nullptr);
    pthread_cond_init(&released_, nullptr);
    for (Memory &memory : memory_) memory = Memory{0u, 0u, 0u, -1, nullptr, -1, false, 0u};
    table_.abi_version = AV_EXECUTOR_ABI_VERSION;
    table_.opaque = this;
    table_.sub_apis = [](void *opaque) { return mock_of(opaque).sub_apis_; };
    table_.allocate_memory = [](void *opaque, std::uint32_t domain, std::uint64_t key, std::uint64_t size,
                                std::uint32_t request, AvHostMemory *memory) -> std::uint32_t {
        if (memory == nullptr) return 1u;
        return mock_of(opaque).allocate(domain, key, size, request, *memory);
    };
    table_.alias_memory = [](void *, std::uint32_t, std::uint64_t, std::uint32_t, std::uint64_t,
                             AvHostMemory *) -> std::uint32_t { return 1u; };
    table_.free_memory = [](void *opaque, std::uint32_t domain, std::uint64_t key) {
        MockExecutorOps &mock = mock_of(opaque);
        Lock             lock(mock.lock_);
        Memory *const    memory = mock.held_memory_of(domain, key);
        if (memory == nullptr) return;
        mock.memory_frees_++;
        memory->held = false;
        mock.settle(*memory);
    };
    /* An import names memory of the executor's own domain the device still
     * holds: memory it already freed is gone to a new import, as the
     * library's own source refuses it. */
    table_.import_blob = [](void *opaque, AvExecutor *handle, std::uint64_t blob_id,
                            std::uint64_t memory_key) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        Executor *const  executor = mock.executor_of(handle);
        if (executor == nullptr) return AV_UNDECODABLE;
        Lock lock(mock.lock_);
        if (mock.fires(mock.refuse_import_)) return AV_UNDECODABLE;
        Memory *const memory = mock.held_memory_of(executor->domain, memory_key);
        if (memory == nullptr) return AV_UNDECODABLE;
        for (Import &import : mock.imports_) {
            if (import.live) continue;
            import = Import{blob_id, memory_key, memory, handle, true};
            memory->imports++;
            mock.imported_++;
            return AV_EXECUTED;
        }
        return AV_UNDECODABLE;
    };
    table_.release_blob = [](void *opaque, AvExecutor *handle, std::uint64_t blob_id) {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr) return;
        Lock lock(mock.lock_);
        mock.released_blobs_++;
        for (Import &import : mock.imports_)
            if (import.live && import.executor == handle && import.blob_id == blob_id) mock.release_import(import);
    };
    table_.imported_handle = [](void *opaque, AvExecutor *handle, std::uint64_t blob_id) -> std::uint64_t {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr) return 0u;
        Lock lock(mock.lock_);
        if (mock.withhold_handle_ != 0u && --mock.withhold_handle_ == 0u) return 0u;
        for (const Import &import : mock.imports_) {
            if (!import.live || import.executor != handle || import.blob_id != blob_id) continue;
            return mock.answered_handle_ != 0u ? mock.answered_handle_ : blob_id | handle_bit;
        }
        return 0u;
    };
    table_.set_signal = [](void *opaque, AvExecutor *executor, void (*signal)(void *),
                           void *context) {
        MockExecutorOps &mock = mock_of(opaque);
        Lock             lock(mock.lock_);
        static_cast<void>(executor);
        mock.signals_set_++;
        mock.signal_ = signal;
        mock.signal_context_ = context;
    };
    table_.open_domain = [](void *opaque, const std::uint8_t *gpu_uuid) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        Lock             lock(mock.lock_);
        if (mock.refuse_domain_ != 0u && --mock.refuse_domain_ == 0u) return 0u;
        mock.last_domain_gpu_ = gpu_uuid != nullptr ? gpu_uuid[0] : 0u;
        return domain_bit | ++mock.domains_opened_;
    };
    table_.close_domain = [](void *opaque, std::uint32_t domain) {
        MockExecutorOps &mock = mock_of(opaque);
        Lock             lock(mock.lock_);
        mock.domains_closed_++;
        mock.last_closed_domain_ = domain;
        for (Share &share : mock.shares_)
            if (share.domain == domain) share.live = false;
    };
    /* One host object shared between executors (feature 053), kept as the
     * library keeps it: an id per domain, its kind and size, and who opened
     * it with what handle. */
    table_.share_object = [](void *opaque, AvExecutor *handle, std::uint64_t object_id, std::uint32_t kind,
                             std::uint64_t size, std::uint64_t wire_handle) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr) return AV_SHARED_REFUSED;
        Lock lock(mock.lock_);
        if (wire_handle == 0u || kind < AV_SHARED_MEMORY || kind > AV_SHARED_NATIVE_FENCE) return AV_SHARED_REFUSED;
        if (mock.refuse_share_ != 0u && --mock.refuse_share_ == 0u) return AV_SHARED_REFUSED;
        for (const Share &share : mock.shares_) {
            if (!share.live || share.object_id != object_id) continue;
            const bool again = share.publisher == handle && share.kind == kind && share.size == size;
            return again ? AV_SHARED_DONE : AV_SHARED_CONFLICT;
        }
        for (Share &share : mock.shares_) {
            if (share.live) continue;
            share = Share{object_id, mock.last_options_.domain, kind, size, handle, true};
            mock.shared_objects_++;
            return AV_SHARED_DONE;
        }
        return AV_SHARED_REFUSED;
    };
    table_.open_object = [](void *opaque, AvExecutor *handle, std::uint64_t object_id, std::uint32_t kind,
                            std::uint64_t size, std::uint64_t *wire_handle) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr || wire_handle == nullptr) return AV_SHARED_REFUSED;
        *wire_handle = 0u;
        Lock         lock(mock.lock_);
        const Share *found = nullptr;
        for (const Share &share : mock.shares_)
            if (share.live && share.object_id == object_id) found = &share;
        if (found == nullptr) return AV_SHARED_UNKNOWN;
        if (found->kind != kind || found->size != size) return AV_SHARED_MISMATCHED;
        if (found->publisher == handle) return AV_SHARED_CONFLICT;
        for (const Open &open : mock.opens_)
            if (open.live && open.object_id == object_id && open.executor == handle) return AV_SHARED_CONFLICT;
        if (mock.refuse_open_ != 0u && --mock.refuse_open_ == 0u) return AV_SHARED_REFUSED;
        mock.opened_objects_++;
        if (mock.withhold_open_handle_ != 0u && --mock.withhold_open_handle_ == 0u) return AV_SHARED_DONE;
        for (Open &open : mock.opens_) {
            if (open.live) continue;
            open = Open{object_id, handle, object_bit | ++mock.next_object_, true};
            *wire_handle = open.wire_handle;
            return AV_SHARED_DONE;
        }
        return AV_SHARED_REFUSED;
    };
    table_.close_object = [](void *opaque, AvExecutor *handle, std::uint64_t object_id) {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr) return;
        Lock lock(mock.lock_);
        mock.closed_objects_++;
        for (Open &open : mock.opens_)
            if (open.live && open.object_id == object_id && open.executor == handle) open.live = false;
    };
    table_.release_shared = [](void *opaque, std::uint32_t domain, std::uint64_t object_id) {
        MockExecutorOps &mock = mock_of(opaque);
        Lock             lock(mock.lock_);
        mock.shared_releases_++;
        mock.last_shared_release_ = object_id;
        mock.last_domain_ = domain;
        for (Share &share : mock.shares_)
            if (share.live && share.domain == domain && (object_id == 0u || share.object_id == object_id))
                share.live = false;
    };
    table_.read_image = [](void *opaque, std::uint32_t domain, std::uint64_t, void *, std::uint32_t, std::uint32_t,
                           std::uint32_t) -> std::uint32_t {
        mock_of(opaque).last_domain_ = domain;
        return AV_UNKNOWN;
    };
    table_.export_image = [](void *opaque, std::uint32_t domain, std::uint64_t, AvExportedImage *) -> std::uint32_t {
        mock_of(opaque).last_domain_ = domain;
        return AV_UNKNOWN;
    };
    table_.map_frame = [](void *opaque, std::uint32_t domain, std::uint64_t surface, std::uint32_t point,
                          std::uint32_t, AvFrameView *view) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        mock.last_domain_ = domain;
        if (view == nullptr || surface == 0u || surface != mock.frame_surface_ || point != mock.frame_point_)
            return AV_UNKNOWN;
        *view = mock.frame_;
        return AV_EXECUTED;
    };
    table_.unmap_frame = [](void *opaque, std::uint32_t domain, std::uint64_t surface, std::uint32_t point) {
        MockExecutorOps &mock = mock_of(opaque);
        mock.last_domain_ = domain;
        if (surface == mock.frame_surface_ && point == mock.frame_point_) mock.frame_surface_ = 0u;
        mock.frame_unmaps_++;
    };
    table_.copy_frame = [](void *opaque, std::uint32_t domain, std::uint64_t surface, std::uint32_t point,
                           std::uint32_t, const AvFrameCopy *copy) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        mock.last_domain_ = domain;
        if (copy == nullptr || surface == 0u || surface != mock.frame_surface_ || point != mock.frame_point_)
            return AV_UNKNOWN;
        return mock.frame_copy_answer_;
    };
    table_.export_surface = [](void *opaque, std::uint32_t domain, std::uint64_t surface, std::uint32_t,
                               AvExportedImage *image) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        mock.last_domain_ = domain;
        if (image == nullptr || surface == 0u || surface != mock.surface_) return AV_UNKNOWN;
        *image = mock.surface_image_;
        return AV_EXECUTED;
    };
    table_.sync_surface = [](void *opaque, std::uint32_t domain, std::uint64_t surface, std::uint32_t,
                             std::uint64_t *writes, int *fd) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        mock.last_domain_ = domain;
        if (writes == nullptr || fd == nullptr || surface == 0u || surface != mock.surface_) return AV_UNKNOWN;
        *writes = mock.surface_writes_;
        *fd = mock.surface_fence_;
        return AV_EXECUTED;
    };
    table_.end_batch = [](void *, AvExecutor *) {};
    table_.poll = [](void *opaque, AvExecutor *) -> std::uint32_t {
        return mock_of(opaque).host_lost_.load() ? AV_POLL_LOST : AV_POLL_IDLE;
    };
    table_.lose_device = [](void *opaque, AvExecutor *) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        if (!mock.losable_.load()) return AV_UNKNOWN;
        mock.lose_host_device();
        return AV_EXECUTED;
    };
    table_.gpu_progress = [](void *, AvExecutor *, std::uint64_t *marked, std::uint64_t *reached) {
        *marked = 0u;
        *reached = 0u;
    };
    table_.create = [](void *opaque, std::uint32_t sub_api, const AvContextOptions *options,
                       AvExecutor *share) -> AvExecutor * {
        MockExecutorOps &mock = mock_of(opaque);
        {
            Lock lock(mock.lock_);
            mock.option_creates_++;
            mock.last_options_ = options != nullptr ? *options : AvContextOptions{};
            mock.last_share_ = share;
            mock.creates_waiting_++;
            while (mock.creates_held_) pthread_cond_wait(&mock.released_, &mock.lock_);
            mock.creates_waiting_--;
        }
        /* Read after the hold: the share must still be alive when a create using it returns. */
        const bool live = mock.alive(share);
        Lock       lock(mock.lock_);
        mock.last_share_live_ = live;
        mock.last_sub_api_ = sub_api;
        if (mock.fail_create_ != 0u && --mock.fail_create_ == 0u) return nullptr;
        for (Executor &executor : mock.executors_) {
            if (executor.live) continue;
            executor = Executor{pthread_self(), mock.last_options_.domain, true};
            mock.created_++;
            return static_cast<AvExecutor *>(static_cast<void *>(&executor));
        }
        return nullptr;
    };
    table_.destroy = [](void *opaque, AvExecutor *handle) {
        MockExecutorOps &mock = mock_of(opaque);
        Executor *const executor = mock.executor_of(handle);
        if (executor == nullptr) return;
        Lock lock(mock.lock_);
        executor->live = false;
        mock.destroyed_++;
        /* Destroy releases whatever the device never did (av_executor.h). */
        for (Import &import : mock.imports_)
            if (import.live && import.executor == handle) mock.release_import(import);
        for (Open &open : mock.opens_)
            if (open.executor == handle) open.live = false;
    };
    table_.execute = [](void *opaque, AvExecutor *handle, std::uint32_t id, const std::uint8_t *payload,
                        std::size_t size, std::uint8_t *reply, std::size_t capacity,
                        std::size_t *reply_size) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        *reply_size = 0u;
        if (mock.executor_of(handle) == nullptr) return AV_DEVICE_LOST;
        {
            Lock lock(mock.lock_);
            mock.waiting_++;
            while (mock.held_) pthread_cond_wait(&mock.released_, &mock.lock_);
            mock.waiting_--;
            if (mock.executed_ < journal_capacity) {
                mock.ids_[mock.executed_] = id;
                mock.sizes_[mock.executed_] = static_cast<std::uint32_t>(size);
            }
            mock.executed_++;
            std::free(mock.kept_);
            mock.kept_ = nullptr;
            mock.kept_length_ = 0u;
        }
        switch (id) {
        case unknown_id: return AV_UNKNOWN;
        case undecodable_id: return AV_UNDECODABLE;
        case lost_id: return AV_DEVICE_LOST;
        case garbage_id: return 99u;
        case oversized_id: *reply_size = capacity + 1u; return AV_EXECUTED;
        default: break;
        }
        *reply_size = size;
        if (size <= capacity) {
            if (size != 0u) std::memcpy(reply, payload, size);
            return AV_EXECUTED;
        }
        if (reply == nullptr) return AV_EXECUTED;
        Lock lock(mock.lock_);
        mock.kept_ = static_cast<std::uint8_t *>(std::malloc(size));
        if (mock.kept_ != nullptr) std::memcpy(mock.kept_, payload, size);
        mock.kept_length_ = mock.kept_ != nullptr ? size : 0u;
        return AV_EXECUTED;
    };
    table_.answer_part = [](void *opaque, AvExecutor *, std::uint64_t offset, std::uint8_t *out,
                            std::size_t capacity, std::uint64_t *length) -> std::size_t {
        MockExecutorOps &mock = mock_of(opaque);
        Lock             lock(mock.lock_);
        *length = mock.kept_length_;
        if (offset >= mock.kept_length_) return 0u;
        const std::size_t left = mock.kept_length_ - static_cast<std::size_t>(offset);
        const std::size_t copied = left < capacity ? left : capacity;
        std::memcpy(out, mock.kept_ + offset, copied);
        return copied;
    };
}

MockExecutorOps::~MockExecutorOps()
{
    release();
    std::free(kept_);
    for (Memory &memory : memory_) {
        if (memory.fd < 0) continue;
        ::munmap(memory.bytes, static_cast<std::size_t>(memory.size));
        ::close(memory.fd);
    }
    pthread_cond_destroy(&released_);
    pthread_mutex_destroy(&lock_);
}

void MockExecutorOps::hold() { Lock lock(lock_); held_ = true; }

void MockExecutorOps::release()
{
    Lock lock(lock_);
    held_ = false;
    pthread_cond_broadcast(&released_);
}

std::uint32_t MockExecutorOps::waiting() { Lock lock(lock_); return waiting_; }

MockExecutorOps::Executor *MockExecutorOps::executor_of(AvExecutor *handle)
{
    Lock lock(lock_);
    for (Executor &executor : executors_) {
        if (static_cast<void *>(&executor) != static_cast<void *>(handle)) continue;
        if (!executor.live || pthread_equal(executor.thread, pthread_self()) == 0) misuses_++;
        return executor.live ? &executor : nullptr;
    }
    misuses_++;
    return nullptr;
}

const AvExecutorOps &MockExecutorOps::table() const { return table_; }
AvExecutorOps       &MockExecutorOps::editable() { return table_; }

void          MockExecutorOps::fail_create_at(std::uint32_t nth) { Lock lock(lock_); fail_create_ = nth; }
void MockExecutorOps::hold_creates()
{
    Lock lock(lock_);
    creates_held_ = true;
}
void MockExecutorOps::release_creates()
{
    Lock lock(lock_);
    creates_held_ = false;
    pthread_cond_broadcast(&released_);
}
std::uint32_t     MockExecutorOps::creates_waiting() { Lock lock(lock_); return creates_waiting_; }
std::uint32_t     MockExecutorOps::option_creates() { Lock lock(lock_); return option_creates_; }
AvContextOptions MockExecutorOps::last_options() { Lock lock(lock_); return last_options_; }
AvExecutor      *MockExecutorOps::last_share() { Lock lock(lock_); return last_share_; }
bool              MockExecutorOps::last_share_live() { Lock lock(lock_); return last_share_live_; }

/* Whether `handle` is a live executor of this table, asked from any thread:
 * sharing reads another context's executor without calling into it. */
bool MockExecutorOps::alive(AvExecutor *handle)
{
    Lock lock(lock_);
    for (const Executor &executor : executors_)
        if (static_cast<const void *>(&executor) == static_cast<const void *>(handle)) return executor.live;
    return false;
}
std::uint32_t MockExecutorOps::created() { Lock lock(lock_); return created_; }
std::uint32_t MockExecutorOps::destroyed() { Lock lock(lock_); return destroyed_; }
std::uint32_t MockExecutorOps::live() { Lock lock(lock_); return created_ - destroyed_; }
std::uint32_t MockExecutorOps::executed() { Lock lock(lock_); return executed_; }
std::uint32_t MockExecutorOps::id_at(std::uint32_t index) { Lock lock(lock_); return index < journal_capacity ? ids_[index] : 0u; }
std::uint32_t MockExecutorOps::size_at(std::uint32_t index) { Lock lock(lock_); return index < journal_capacity ? sizes_[index] : 0u; }
std::uint32_t MockExecutorOps::last_sub_api() { Lock lock(lock_); return last_sub_api_; }
void          MockExecutorOps::refuse_domain_at(std::uint32_t nth) { Lock lock(lock_); refuse_domain_ = nth; }
std::uint32_t MockExecutorOps::domains_opened() { Lock lock(lock_); return domains_opened_; }
std::uint32_t MockExecutorOps::domains_closed() { Lock lock(lock_); return domains_closed_; }
std::uint32_t MockExecutorOps::last_opened_domain() { Lock lock(lock_); return domain_bit | domains_opened_; }
std::uint32_t MockExecutorOps::last_closed_domain() { Lock lock(lock_); return last_closed_domain_; }
std::uint8_t  MockExecutorOps::last_domain_gpu() { Lock lock(lock_); return last_domain_gpu_; }
std::uint32_t MockExecutorOps::misuses() { return misuses_.load(); }

bool MockExecutorOps::fires(std::uint32_t &countdown)
{
    return countdown != 0u && --countdown == 0u;
}

MockExecutorOps::Memory *MockExecutorOps::held_memory_of(std::uint32_t domain, std::uint64_t key)
{
    for (Memory &memory : memory_)
        if (memory.fd >= 0 && memory.held && memory.domain == domain && memory.key == key) return &memory;
    return nullptr;
}

void MockExecutorOps::settle(Memory &memory)
{
    if (memory.held || memory.imports != 0u) return;
    ::munmap(memory.bytes, static_cast<std::size_t>(memory.size));
    ::close(memory.fd);
    memory = Memory{0u, 0u, 0u, -1, nullptr, -1, false, 0u};
}

void MockExecutorOps::release_import(Import &import)
{
    import.live = false;
    import.memory->imports--;
    settle(*import.memory);
}

/* A real allocation, so a test reads through it what the guest wrote into
 * the window: the device is handed its own duplicate of the descriptor, as
 * the library's source hands one, and closes it with the blob. */
std::uint32_t MockExecutorOps::allocate(std::uint32_t domain, std::uint64_t key, std::uint64_t size,
                                        std::uint32_t request, AvHostMemory &answer)
{
    Lock lock(lock_);
    memory_allocations_++;
    last_memory_request_ = MemoryRequest{domain, key, size, request};
    if (fires(refuse_memory_) || size == 0u || held_memory_of(domain, key) != nullptr) return 1u;
    std::uint64_t made = size;
    if (memory_rounding_ != 0u) made = (size + memory_rounding_ - 1u) / memory_rounding_ * memory_rounding_;
    Memory *slot = nullptr;
    for (Memory &memory : memory_)
        if (memory.fd < 0 && slot == nullptr) slot = &memory;
    if (slot == nullptr) return 1u;
    const int fd = ::memfd_create("mock executor memory", MFD_CLOEXEC);
    if (fd < 0) return 1u;
    void *bytes = MAP_FAILED;
    if (::ftruncate(fd, static_cast<off_t>(made)) == 0)
        bytes = ::mmap(nullptr, static_cast<std::size_t>(made), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bytes == MAP_FAILED) {
        ::close(fd);
        return 1u;
    }
    const int handed = fires(withhold_memory_descriptor_) ? -1 : ::fcntl(fd, F_DUPFD_CLOEXEC, 0);
    *slot = Memory{domain, key, made, fd, static_cast<std::uint8_t *>(bytes), handed, true, 0u};
    answer.fd = handed;
    answer.caching = memory_caching_;
    answer.size = fires(shrink_memory_) ? made - 4096u : made;
    return 0u;
}

void MockExecutorOps::refuse_memory_at(std::uint32_t nth) { Lock lock(lock_); refuse_memory_ = nth; }
void MockExecutorOps::withhold_memory_descriptor_at(std::uint32_t nth)
{
    Lock lock(lock_);
    withhold_memory_descriptor_ = nth;
}
void MockExecutorOps::shrink_memory_at(std::uint32_t nth) { Lock lock(lock_); shrink_memory_ = nth; }
void MockExecutorOps::round_memory_to(std::uint64_t bytes) { Lock lock(lock_); memory_rounding_ = bytes; }
void MockExecutorOps::set_memory_caching(std::uint32_t caching) { Lock lock(lock_); memory_caching_ = caching; }
std::uint32_t MockExecutorOps::memory_allocations() { Lock lock(lock_); return memory_allocations_; }
std::uint32_t MockExecutorOps::memory_frees() { Lock lock(lock_); return memory_frees_; }

std::uint32_t MockExecutorOps::held_memory()
{
    Lock          lock(lock_);
    std::uint32_t count = 0u;
    for (const Memory &memory : memory_) count += memory.fd >= 0 && memory.held ? 1u : 0u;
    return count;
}

std::uint32_t MockExecutorOps::live_memory()
{
    Lock          lock(lock_);
    std::uint32_t count = 0u;
    for (const Memory &memory : memory_) count += memory.fd >= 0 ? 1u : 0u;
    return count;
}

MockExecutorOps::MemoryRequest MockExecutorOps::last_memory_request() { Lock lock(lock_); return last_memory_request_; }

/* The held allocation under the key, else the newest one an import still
 * keeps alive after the device let the key go. */
std::uint8_t *MockExecutorOps::memory_bytes(std::uint32_t domain, std::uint64_t key)
{
    Lock lock(lock_);
    for (const Memory &memory : memory_)
        if (memory.fd >= 0 && memory.domain == domain && memory.key == key) return memory.bytes;
    return nullptr;
}

int MockExecutorOps::answered_descriptor(std::uint32_t domain, std::uint64_t key)
{
    Lock lock(lock_);
    for (const Memory &memory : memory_)
        if (memory.fd >= 0 && memory.domain == domain && memory.key == key) return memory.answered;
    return -1;
}

std::uint32_t MockExecutorOps::imported()
{
    Lock lock(lock_);
    return imported_;
}

std::uint32_t MockExecutorOps::released()
{
    Lock lock(lock_);
    return released_blobs_;
}

}  // namespace av

namespace av {

void MockExecutorOps::refuse_import_at(std::uint32_t nth) { Lock lock(lock_); refuse_import_ = nth; }
void MockExecutorOps::withhold_handle_at(std::uint32_t nth) { Lock lock(lock_); withhold_handle_ = nth; }
void MockExecutorOps::answer_handle(std::uint64_t handle) { Lock lock(lock_); answered_handle_ = handle; }

bool MockExecutorOps::import_of(std::uint64_t blob_id, std::uint64_t &memory_key)
{
    Lock lock(lock_);
    for (const Import &import : imports_) {
        if (!import.live || import.blob_id != blob_id) continue;
        memory_key = import.memory_key;
        return true;
    }
    return false;
}

std::uint32_t MockExecutorOps::live_imports()
{
    Lock lock(lock_);
    std::uint32_t count = 0u;
    for (const Import &import : imports_) count += import.live ? 1u : 0u;
    return count;
}

std::uint32_t MockExecutorOps::signals_set()
{
    Lock lock(lock_);
    return signals_set_;
}

bool MockExecutorOps::signal_bound() const { return signal_ != nullptr; }

void MockExecutorOps::lose_host_device()
{
    host_lost_.store(true);
    losses_.fetch_add(1u);
}

void MockExecutorOps::set_losable(bool losable) { losable_.store(losable); }

std::uint32_t MockExecutorOps::losses() { return losses_.load(); }

bool MockExecutorOps::fire_signal()
{
    void (*signal)(void *) = nullptr;
    void *context = nullptr;
    {
        Lock lock(lock_);
        signal = signal_;
        context = signal_context_;
    }
    if (signal == nullptr) return false;
    /* Outside the lock, as a host API's own thread would call it: the pad it
     * reaches takes no lock of this library's. */
    signal(context);
    return true;
}


void MockExecutorOps::set_abi_version(std::uint32_t version)
{
    table_.abi_version = version == 0u ? AV_EXECUTOR_ABI_VERSION : version;
}

void MockExecutorOps::refuse_share_at(std::uint32_t nth) { Lock lock(lock_); refuse_share_ = nth; }
void MockExecutorOps::refuse_open_at(std::uint32_t nth) { Lock lock(lock_); refuse_open_ = nth; }
void MockExecutorOps::withhold_open_handle_at(std::uint32_t nth) { Lock lock(lock_); withhold_open_handle_ = nth; }
std::uint32_t MockExecutorOps::shared_objects() { Lock lock(lock_); return shared_objects_; }
std::uint32_t MockExecutorOps::opened_objects() { Lock lock(lock_); return opened_objects_; }
std::uint32_t MockExecutorOps::closed_objects() { Lock lock(lock_); return closed_objects_; }

std::uint32_t MockExecutorOps::live_opens()
{
    Lock          lock(lock_);
    std::uint32_t count = 0u;
    for (const Open &open : opens_)
        if (open.live) count++;
    return count;
}

bool MockExecutorOps::opened_as(AvExecutor *executor, std::uint64_t object_id, std::uint64_t &wire_handle)
{
    Lock lock(lock_);
    for (const Open &open : opens_) {
        if (!open.live || open.object_id != object_id || open.executor != executor) continue;
        wire_handle = open.wire_handle;
        return true;
    }
    return false;
}

void MockExecutorOps::serve_frame(std::uint64_t surface, std::uint32_t point, const AvFrameView &view)
{
    frame_surface_ = surface;
    frame_point_ = point;
    frame_ = view;
}

std::uint32_t MockExecutorOps::frame_unmaps() const { return frame_unmaps_; }
void          MockExecutorOps::copy_frames_with(std::uint32_t answer) { frame_copy_answer_ = answer; }

void MockExecutorOps::serve_surface(std::uint64_t surface, const AvExportedImage &image, std::uint64_t writes,
                                    int fence)
{
    surface_ = surface;
    surface_image_ = image;
    surface_writes_ = writes;
    surface_fence_ = fence;
}

std::uint32_t MockExecutorOps::last_domain() const { return last_domain_; }
std::uint32_t MockExecutorOps::shared_releases() { Lock lock(lock_); return shared_releases_; }
std::uint64_t MockExecutorOps::last_shared_release() { Lock lock(lock_); return last_shared_release_; }

}  // namespace av
