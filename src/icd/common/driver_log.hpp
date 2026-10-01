/* A driver's lines (constitution IX), each prefixed with the driver's name -
 * "avvk: ", "avgl: ", "avcl: ", "avumd: " - and the ones a caller would
 * otherwise write on every call written once per key instead.
 *
 * A key is a string literal - an entry point's name, a slot's, a message -
 * so one key is one pointer, with a code beside it (an error, an HRESULT) for
 * a key that may fail more than one way. The table is append-only and takes
 * no lock, because a driver logs from inside its own locks: two threads
 * meeting the same new key at the same moment may both write its line, which
 * a log can afford. Past max_keys a line is not written at all rather than
 * flooding the log. */
#ifndef AV_ICD_DRIVER_LOG_HPP
#define AV_ICD_DRIVER_LOG_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::icd {

class LogSink;

class DriverLog {
public:
    static constexpr uint32_t max_keys = 256u;
    static constexpr size_t   line_bytes = 192u;

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

    /* True the first time `key` is met with `code`, and from then on false. */
    [[nodiscard]] bool first(const char *key, int64_t code);
    /* write(what), the first time `what` is met. */
    void               once(const char *what);
    /* name(what, name), the first time `name` is met with `what`. */
    void               once_name(const char *what, const char *name);
    uint32_t           keys() const;

private:

    LogSink    &sink_;
    const char *prefix_;
    const char *keys_[max_keys];
    int64_t     codes_[max_keys];
    /* Whether a row's key and code are written: a reader passes a row it is
     * still being written into. */
    uint8_t     ready_[max_keys];
    uint32_t    count_;
};

}  // namespace av::icd

#endif  // AV_ICD_DRIVER_LOG_HPP
