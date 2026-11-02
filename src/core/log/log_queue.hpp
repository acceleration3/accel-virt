#ifndef AV_LOG_QUEUE_HPP
#define AV_LOG_QUEUE_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class LogQueue {
public:
    LogQueue(uint8_t *storage, size_t capacity);
    LogQueue(const LogQueue &) = delete;
    LogQueue &operator=(const LogQueue &) = delete;

    bool     append(const char *line);
    size_t   take(uint8_t *out, size_t room);
    size_t   queued() const;
    uint64_t take_dropped();

private:
    uint8_t *storage_;
    size_t   capacity_;
    size_t   head_;
    size_t   count_;
    uint64_t dropped_;
};

}

#endif
