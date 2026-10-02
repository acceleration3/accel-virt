/* Log lines waiting for a slow sink, in memory the owner provides: a writer
 * queues a whole line or nothing, and a drainer takes the bytes out oldest
 * first. The driver's sink is a port QEMU services a byte per VM exit, a
 * millisecond a line, which no DPC may wait for. The owner serializes both
 * sides; the queue holds no lock of its own. */
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

    /* `line` and a newline after it; false, with nothing queued and the line
     * counted as dropped, when both do not fit. */
    bool     append(const char *line);
    /* Up to `room` bytes, oldest first, taken out of the queue. */
    size_t   take(uint8_t *out, size_t room);
    size_t   queued() const;
    /* The lines dropped since the last call, which starts the count again. */
    uint64_t take_dropped();

private:
    uint8_t *storage_;
    size_t   capacity_;
    size_t   head_;
    size_t   count_;
    uint64_t dropped_;
};

}  // namespace av

#endif  // AV_LOG_QUEUE_HPP
