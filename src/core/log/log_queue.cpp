#include "core/log/log_queue.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

LogQueue::LogQueue(uint8_t *storage, size_t capacity)
    : storage_(storage), capacity_(storage != nullptr ? capacity : 0u), head_(0u), count_(0u), dropped_(0u)
{
    AV_SCOPE("LogQueue::LogQueue");
}

bool LogQueue::append(const char *line)
{
    AV_SCOPE("LogQueue::append");
    size_t length = 0u;
    while (line[length] != '\0') length++;
    if (length + 1u > capacity_ - count_) {
        dropped_++;
        return false;
    }
    size_t tail = (head_ + count_) % capacity_;
    for (size_t index = 0u; index <= length; index++) {
        storage_[tail] = index < length ? static_cast<uint8_t>(line[index]) : static_cast<uint8_t>('\n');
        tail = (tail + 1u) % capacity_;
    }
    count_ += length + 1u;
    return true;
}

size_t LogQueue::take(uint8_t *out, size_t room)
{
    AV_SCOPE("LogQueue::take");
    const size_t taken = room < count_ ? room : count_;
    for (size_t index = 0u; index < taken; index++) {
        out[index] = storage_[head_];
        head_ = (head_ + 1u) % capacity_;
    }
    count_ -= taken;
    return taken;
}

size_t LogQueue::queued() const
{
    AV_SCOPE("LogQueue::queued");
    return count_;
}

uint64_t LogQueue::take_dropped()
{
    AV_SCOPE("LogQueue::take_dropped");
    const uint64_t dropped = dropped_;
    dropped_ = 0u;
    return dropped;
}

}  // namespace av
