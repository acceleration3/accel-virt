#include "harness/executor/mock_executor_ops.hpp"

#include "harness/host/mock_host.hpp"

#include <cstring>

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
    : table_{}, sub_apis_(sub_apis), import_kinds_(0u), imported_(0u), released_blobs_(0u), lock_(), released_(), held_(false), waiting_(0u), executors_{}, fail_create_(0u), created_(0u), destroyed_(0u),
      executed_(0u), ids_{}, sizes_{}, last_sub_api_(0u), last_uuid_byte_(0u), misuses_(0u), imports_{},
      refuse_import_(0u), withhold_handle_(0u), answered_handle_(0u), option_creates_(0u), signals_set_(0u), signal_(nullptr), signal_context_(nullptr), last_options_{},
      last_share_(nullptr), last_share_live_(false), creates_held_(false), creates_waiting_(0u),
      descriptors_(nullptr), export_handle_kind_(AV_SHARED_HANDLE_DMA_BUF), refuse_export_(0u),
      lie_on_export_(0u),
      refuse_object_import_(0u), withhold_object_handle_(0u), exported_objects_(0u), imported_objects_(0u),
      released_objects_(0u), next_object_(0u), objects_{}
{
    pthread_mutex_init(&lock_, nullptr);
    pthread_cond_init(&released_, nullptr);
    table_.abi_version = AV_EXECUTOR_ABI_VERSION;
    table_.opaque = this;
    table_.sub_apis = [](void *opaque) { return mock_of(opaque).sub_apis_; };
    table_.import_kinds = [](void *opaque, std::uint32_t, const std::uint8_t *) {
        return mock_of(opaque).import_kinds_;
    };
    table_.import_blob = [](void *opaque, AvExecutor *handle, std::uint64_t blob_id,
                            const AvBlobHandle *blob) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr || blob == nullptr) return AV_UNDECODABLE;
        Lock lock(mock.lock_);
        if (mock.refuse_import_ != 0u && --mock.refuse_import_ == 0u) return AV_UNDECODABLE;
        for (Import &import : mock.imports_) {
            if (import.live) continue;
            import = Import{blob_id, *blob, true};
            break;
        }
        mock.imported_++;
        return AV_EXECUTED;
    };
    table_.release_blob = [](void *opaque, AvExecutor *handle, std::uint64_t blob_id) {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr) return;
        Lock lock(mock.lock_);
        mock.released_blobs_++;
        for (Import &import : mock.imports_)
            if (import.live && import.blob_id == blob_id) import.live = false;
    };
    table_.imported_handle = [](void *opaque, AvExecutor *handle, std::uint64_t blob_id) -> std::uint64_t {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr) return 0u;
        Lock lock(mock.lock_);
        if (mock.withhold_handle_ != 0u && --mock.withhold_handle_ == 0u) return 0u;
        for (const Import &import : mock.imports_) {
            if (!import.live || import.blob_id != blob_id) continue;
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
    /* Version 6: one host object moved between two executors (feature 053).
     * The descriptor is a real one of the host's, because the device closes it
     * when the last holder lets go and "closed exactly once" is the claim
     * these tests are for. */
    table_.export_object = [](void *opaque, AvExecutor *handle, std::uint32_t kind,
                              std::uint64_t wire_handle, std::uint32_t *handle_kind,
                              int *fd) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr || handle_kind == nullptr || fd == nullptr)
            return AV_UNKNOWN;
        Lock lock(mock.lock_);
        if (wire_handle == 0u || kind < AV_SHARED_MEMORY || kind > AV_SHARED_NATIVE_FENCE)
            return AV_UNKNOWN;
        if (mock.refuse_export_ != 0u && --mock.refuse_export_ == 0u) return AV_UNKNOWN;
        if (mock.descriptors_ == nullptr) return AV_UNKNOWN;
        if (mock.lie_on_export_ != 0u && --mock.lie_on_export_ == 0u) {
            /* A real descriptor with a kind that is not one: the half of a
             * lying library the caller cannot tell from a good answer without
             * looking at what came back. */
            const int lied = mock.descriptors_->open_descriptor();
            if (lied < 0) return AV_UNKNOWN;
            *handle_kind = 0u;
            *fd = lied;
            mock.exported_objects_++;
            return AV_EXECUTED;
        }
        const int made = mock.descriptors_->open_descriptor();
        if (made < 0) return AV_UNKNOWN;
        *handle_kind = mock.export_handle_kind_;
        *fd = made;
        mock.exported_objects_++;
        return AV_EXECUTED;
    };
    table_.import_object = [](void *opaque, AvExecutor *handle, std::uint32_t kind,
                              std::uint32_t handle_kind, int fd, std::uint64_t size,
                              std::uint64_t *wire_handle) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr || wire_handle == nullptr) return AV_UNKNOWN;
        Lock lock(mock.lock_);
        if (fd < 0 || size == 0u || kind < AV_SHARED_MEMORY || kind > AV_SHARED_NATIVE_FENCE)
            return AV_UNKNOWN;
        if (mock.refuse_object_import_ != 0u && --mock.refuse_object_import_ == 0u) return AV_UNKNOWN;
        mock.imported_objects_++;
        if (mock.withhold_object_handle_ != 0u && --mock.withhold_object_handle_ == 0u) {
            *wire_handle = 0u;
            return AV_EXECUTED;
        }
        const std::uint64_t made = object_bit | ++mock.next_object_;
        for (SharedObject &object : mock.objects_) {
            if (object.live) continue;
            object = SharedObject{made, kind, handle_kind, size, fd, true};
            *wire_handle = made;
            return AV_EXECUTED;
        }
        return AV_UNKNOWN;
    };
    table_.release_object = [](void *opaque, AvExecutor *handle, std::uint32_t, std::uint64_t wire_handle) {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr) return;
        Lock lock(mock.lock_);
        mock.released_objects_++;
        for (SharedObject &object : mock.objects_)
            if (object.live && object.wire_handle == wire_handle) object.live = false;
    };
    table_.map_frame = [](void *opaque, const std::uint8_t *, std::uint64_t surface, std::uint32_t point,
                          std::uint32_t, AvFrameView *view) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        if (view == nullptr || surface == 0u || surface != mock.frame_surface_ || point != mock.frame_point_)
            return AV_UNKNOWN;
        *view = mock.frame_;
        return AV_EXECUTED;
    };
    table_.unmap_frame = [](void *opaque, const std::uint8_t *, std::uint64_t surface, std::uint32_t point) {
        MockExecutorOps &mock = mock_of(opaque);
        if (surface == mock.frame_surface_ && point == mock.frame_point_) mock.frame_surface_ = 0u;
        mock.frame_unmaps_++;
    };
    table_.release_shared = [](void *opaque, const std::uint8_t *gpu_uuid, std::uint64_t object_id) {
        MockExecutorOps &mock = mock_of(opaque);
        Lock lock(mock.lock_);
        mock.shared_releases_++;
        mock.last_shared_release_ = object_id;
        mock.last_shared_gpu_ = gpu_uuid;
    };
    table_.create_with_options = [](void *opaque, std::uint32_t sub_api, const std::uint8_t *uuid,
                                    const AvContextOptions *options, AvExecutor *share) -> AvExecutor * {
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
        {
            Lock lock(mock.lock_);
            mock.last_share_live_ = live;
        }
        return mock.table_.create(opaque, sub_api, uuid);
    };
    table_.create = [](void *opaque, std::uint32_t sub_api, const std::uint8_t *uuid) -> AvExecutor * {
        MockExecutorOps &mock = mock_of(opaque);
        Lock lock(mock.lock_);
        mock.last_sub_api_ = sub_api;
        mock.last_uuid_byte_ = uuid[0];
        if (mock.fail_create_ != 0u && --mock.fail_create_ == 0u) return nullptr;
        for (Executor &executor : mock.executors_) {
            if (executor.live) continue;
            executor = Executor{pthread_self(), true};
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
        for (Import &import : mock.imports_) import.live = false;
        for (SharedObject &object : mock.objects_) object.live = false;
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
        }
        switch (id) {
        case unknown_id: return AV_UNKNOWN;
        case undecodable_id: return AV_UNDECODABLE;
        case lost_id: return AV_DEVICE_LOST;
        case garbage_id: return 99u;
        case oversized_id: *reply_size = capacity + 1u; return AV_EXECUTED;
        default: break;
        }
        const std::size_t answer = size < capacity ? size : capacity;
        if (answer != 0u) std::memcpy(reply, payload, answer);
        *reply_size = size;
        return AV_EXECUTED;
    };
}

MockExecutorOps::~MockExecutorOps()
{
    release();
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
std::uint8_t  MockExecutorOps::last_uuid_byte() { Lock lock(lock_); return last_uuid_byte_; }
std::uint32_t MockExecutorOps::misuses() { return misuses_.load(); }

void MockExecutorOps::set_import_kinds(std::uint32_t kinds) { import_kinds_ = kinds; }

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

bool MockExecutorOps::import_of(std::uint64_t blob_id, AvBlobHandle &handle)
{
    Lock lock(lock_);
    for (const Import &import : imports_) {
        if (!import.live || import.blob_id != blob_id) continue;
        handle = import.handle;
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


void MockExecutorOps::descriptors_from(MockHost &host)
{
    Lock lock(lock_);
    descriptors_ = &host;
}

void MockExecutorOps::set_abi_version(std::uint32_t version)
{
    table_.abi_version = version == 0u ? AV_EXECUTOR_ABI_VERSION : version;
}

void MockExecutorOps::export_handle_kind(std::uint32_t kind)
{
    Lock lock(lock_);
    export_handle_kind_ = kind;
}

void MockExecutorOps::lie_on_export_at(std::uint32_t nth)
{
    Lock lock(lock_);
    lie_on_export_ = nth;
}

void MockExecutorOps::refuse_export_at(std::uint32_t nth)
{
    Lock lock(lock_);
    refuse_export_ = nth;
}

void MockExecutorOps::refuse_object_import_at(std::uint32_t nth)
{
    Lock lock(lock_);
    refuse_object_import_ = nth;
}

void MockExecutorOps::withhold_object_handle_at(std::uint32_t nth)
{
    Lock lock(lock_);
    withhold_object_handle_ = nth;
}

std::uint32_t MockExecutorOps::exported_objects()
{
    Lock lock(lock_);
    return exported_objects_;
}

std::uint32_t MockExecutorOps::imported_objects()
{
    Lock lock(lock_);
    return imported_objects_;
}

std::uint32_t MockExecutorOps::released_objects()
{
    Lock lock(lock_);
    return released_objects_;
}

void MockExecutorOps::serve_frame(std::uint64_t surface, std::uint32_t point, const AvFrameView &view)
{
    frame_surface_ = surface;
    frame_point_ = point;
    frame_ = view;
}

std::uint32_t MockExecutorOps::frame_unmaps() const
{
    return frame_unmaps_;
}

std::uint32_t MockExecutorOps::shared_releases()
{
    Lock lock(lock_);
    return shared_releases_;
}

std::uint64_t MockExecutorOps::last_shared_release()
{
    Lock lock(lock_);
    return last_shared_release_;
}

const std::uint8_t *MockExecutorOps::last_shared_gpu()
{
    Lock lock(lock_);
    return last_shared_gpu_;
}

std::uint32_t MockExecutorOps::live_objects()
{
    Lock          lock(lock_);
    std::uint32_t count = 0u;
    for (const SharedObject &object : objects_)
        if (object.live) count++;
    return count;
}

bool MockExecutorOps::object_of(std::uint64_t wire_handle, std::uint32_t &kind, std::uint32_t &handle_kind,
                                std::uint64_t &size, int &fd)
{
    Lock lock(lock_);
    for (const SharedObject &object : objects_) {
        if (!object.live || object.wire_handle != wire_handle) continue;
        kind = object.kind;
        handle_kind = object.handle_kind;
        size = object.size;
        fd = object.fd;
        return true;
    }
    return false;
}

}  // namespace av
