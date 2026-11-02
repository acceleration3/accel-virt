#ifndef AV_DRIVER_LOG_HPP
#define AV_DRIVER_LOG_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class LogSink;

class DriverLog {
public:
    static constexpr size_t line_bytes = 192u;

    DriverLog(LogSink &sink, const char *prefix);
    DriverLog(const DriverLog &) = delete;
    DriverLog &operator=(const DriverLog &) = delete;

    void write(const char *what);
    void note(const char *what, uint64_t value);
    void note_hex(const char *what, uint64_t value, uint32_t digits);
    void name(const char *what, const char *name);

private:
    LogSink    &sink_;
    const char *prefix_;
};

}

#endif
