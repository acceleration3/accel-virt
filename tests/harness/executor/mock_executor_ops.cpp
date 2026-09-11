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
    : table_{}, sub_apis_(sub_apis), lock_(), executors_{}, fail_create_(0u), created_(0u), destroyed_(0u),
      executed_(0u), ids_{}, sizes_{}, last_sub_api_(0u), last_uuid_byte_(0u), misuses_(0u)
{
    pthread_mutex_init(&lock_, nullptr);
    table_.abi_version = RVK_EXECUTOR_ABI_VERSION;
    table_.opaque = this;
    table_.sub_apis = [](void *opaque) { return mock_of(opaque).sub_apis_; };
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
    };
    table_.execute = [](void *opaque, RvkExecutor *handle, std::uint32_t id, const std::uint8_t *payload,
                        std::size_t size, std::uint8_t *reply, std::size_t capacity,
                        std::size_t *reply_size) -> std::uint32_t {
        MockExecutorOps &mock = mock_of(opaque);
        *reply_size = 0u;
        if (mock.executor_of(handle) == nullptr) return RVK_DEVICE_LOST;
        {
            Lock lock(mock.lock_);
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

MockExecutorOps::~MockExecutorOps() { pthread_mutex_destroy(&lock_); }

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
std::uint32_t MockExecutorOps::created() { Lock lock(lock_); return created_; }
std::uint32_t MockExecutorOps::destroyed() { Lock lock(lock_); return destroyed_; }
std::uint32_t MockExecutorOps::live() { Lock lock(lock_); return created_ - destroyed_; }
std::uint32_t MockExecutorOps::executed() { Lock lock(lock_); return executed_; }
std::uint32_t MockExecutorOps::id_at(std::uint32_t index) { Lock lock(lock_); return index < journal_capacity ? ids_[index] : 0u; }
std::uint32_t MockExecutorOps::size_at(std::uint32_t index) { Lock lock(lock_); return index < journal_capacity ? sizes_[index] : 0u; }
std::uint32_t MockExecutorOps::last_sub_api() { Lock lock(lock_); return last_sub_api_; }
std::uint8_t  MockExecutorOps::last_uuid_byte() { Lock lock(lock_); return last_uuid_byte_; }
std::uint32_t MockExecutorOps::misuses() { return misuses_.load(); }

}  // namespace av
