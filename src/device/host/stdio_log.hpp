/* Where the device's lines go: a stdio stream - QEMU's stderr, which its log
 * collects - one line at a time, each whole. Every session backend writes its
 * refusals through a DriverLog over one. */
#ifndef AV_STDIO_LOG_HPP
#define AV_STDIO_LOG_HPP

#include "core/log/log_sink.hpp"

#include <stdio.h>

namespace av {

class StdioLog final : public LogSink {
public:
    explicit StdioLog(FILE *stream);

    void write(const char *line) override;

private:
    FILE *stream_;
};

}  // namespace av

#endif  // AV_STDIO_LOG_HPP
