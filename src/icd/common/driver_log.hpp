/* A driver's lines (constitution IX), each prefixed with the driver's name -
 * "avvk: ", "avgl: ", "avcl: ", "avumd: ". KeyedLog adds the lines a caller
 * would otherwise write on every call, written once per key instead. */
#ifndef AV_ICD_DRIVER_LOG_HPP
#define AV_ICD_DRIVER_LOG_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::icd {

class LogSink;

class DriverLog {
public:
    static constexpr size_t line_bytes = 192u;

    /* `prefix` is a literal, kept and written before every line. */
    DriverLog(LogSink &sink, const char *prefix);
    DriverLog(const DriverLog &) = delete;
    DriverLog &operator=(const DriverLog &) = delete;

    void write(const char *what);
    /* "what value", in decimal or as 0x and `digits` hex digits. */
    void note(const char *what, uint64_t value);
    void note_hex(const char *what, uint64_t value, uint32_t digits);
    /* "what name": a refusal and what it named. */
    void name(const char *what, const char *name);

private:
    LogSink    &sink_;
    const char *prefix_;
};

}  // namespace av::icd

#endif  // AV_ICD_DRIVER_LOG_HPP
