#include "harness/executor/mock_executor_ops.hpp"

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
      last_share_(nullptr), last_share_live_(false), creates_held_(false), creates_waiting_(0u)
{
    pthread_mutex_init(&lock_, nullptr);
    pthread_cond_init(&released_, nullptr);
    table_.abi_version = RVK_EXECUTOR_ABI_VERSION;
    table_.opaque = this;
    table_.sub_apis = [](void *opaque) { return mock_of(opaque).sub_apis_; };
    table_.import_kinds = [](void *opaque, std::uint32_t, const std::uint8_t *) {
        return mock_of(opaque).import_kinds_;
    };
    table_.import_blob = [](void *opaque, RvkExecutor *handle, std::uint64_t blob_id,
                            const RvkBlobHandle *blob) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr || blob == nullptr) return RVK_UNDECODABLE;
        Lock lock(mock.lock_);
        if (mock.refuse_import_ != 0u && --mock.refuse_import_ == 0u) return RVK_UNDECODABLE;
        for (Import &import : mock.imports_) {
            if (import.live) continue;
            import = Import{blob_id, *blob, true};
            break;
        }
        mock.imported_++;
        return RVK_EXECUTED;
    };
    table_.release_blob = [](void *opaque, RvkExecutor *handle, std::uint64_t blob_id) {
        MockExecutorOps &mock = mock_of(opaque);
        if (mock.executor_of(handle) == nullptr) return;
        Lock lock(mock.lock_);
        mock.released_blobs_++;
        for (Import &import : mock.imports_)
            if (import.live && import.blob_id == blob_id) import.live = false;
    };
    table_.imported_handle = [](void *opaque, RvkExecutor *handle, std::uint64_t blob_id) -> std::uint64_t {
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
    table_.set_signal = [](void *opaque, RvkExecutor *executor, void (*signal)(void *),
                           void *context) {
        MockExecutorOps &mock = mock_of(opaque);
        Lock             lock(mock.lock_);
        static_cast<void>(executor);
        mock.signals_set_++;
        mock.signal_ = signal;
        mock.signal_context_ = context;
    };
    table_.create_with_options = [](void *opaque, std::uint32_t sub_api, const std::uint8_t *uuid,
                                    const RvkContextOptions *options, RvkExecutor *share) -> RvkExecutor * {
        MockExecutorOps &mock = mock_of(opaque);
        {
            Lock lock(mock.lock_);
            mock.option_creates_++;
            mock.last_options_ = options != nullptr ? *options : RvkContextOptions{};
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
    table_.create = [](void *opaque, std::uint32_t sub_api, const std::uint8_t *uuid) -> RvkExecutor * {
        MockExecutorOps &mock = mock_of(opaque);
        Lock lock(mock.lock_);
        mock.last_sub_api_ = sub_api;
        mock.last_uuid_byte_ = uuid[0];
        if (mock.fail_create_ != 0u && --mock.fail_create_ == 0u) return nullptr;
        for (Executor &executor : mock.executors_) {
            if (executor.live) continue;
            executor = Executor{pthread_self(), true};
            mock.created_++;
            return static_cast<RvkExecutor *>(static_cast<void *>(&executor));
        }
        return nullptr;
    };
    table_.destroy = [](void *opaque, RvkExecutor *handle) {
        MockExecutorOps &mock = mock_of(opaque);
        Executor *const executor = mock.executor_of(handle);
        if (executor == nullptr) return;
        Lock lock(mock.lock_);
        executor->live = false;
        mock.destroyed_++;
        /* Destroy releases whatever the device never did (rvk_executor.h). */
        for (Import &import : mock.imports_) import.live = false;
    };
    table_.execute = [](void *opaque, RvkExecutor *handle, std::uint32_t id, const std::uint8_t *payload,
                        std::size_t size, std::uint8_t *reply, std::size_t capacity,
                        std::size_t *reply_size) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        *reply_size = 0u;
        if (mock.executor_of(handle) == nullptr) return RVK_DEVICE_LOST;
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
        case unknown_id: return RVK_UNKNOWN;
        case undecodable_id: return RVK_UNDECODABLE;
        case lost_id: return RVK_DEVICE_LOST;
        case garbage_id: return 99u;
        case oversized_id: *reply_size = capacity + 1u; return RVK_EXECUTED;
        default: break;
        }
        const std::size_t answer = size < capacity ? size : capacity;
        if (answer != 0u) std::memcpy(reply, payload, answer);
        *reply_size = size;
        return RVK_EXECUTED;
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

MockExecutorOps::Executor *MockExecutorOps::executor_of(RvkExecutor *handle)
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

const RvkExecutorOps &MockExecutorOps::table() const { return table_; }
RvkExecutorOps       &MockExecutorOps::editable() { return table_; }

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
RvkContextOptions MockExecutorOps::last_options() { Lock lock(lock_); return last_options_; }
RvkExecutor      *MockExecutorOps::last_share() { Lock lock(lock_); return last_share_; }
bool              MockExecutorOps::last_share_live() { Lock lock(lock_); return last_share_live_; }
bool              MockExecutorOps::is_live(RvkExecutor *executor) { return alive(executor); }

/* Whether `handle` is a live executor of this table, asked from any thread:
 * sharing reads another context's executor without calling into it. */
bool MockExecutorOps::alive(RvkExecutor *handle)
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

bool MockExecutorOps::import_of(std::uint64_t blob_id, RvkBlobHandle &handle)
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

}  // namespace av
