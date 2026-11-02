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

}

#endif
