/* Where the ICD's lines go (constitution IX): every refusal is written once,
 * where it is decided, with the value that decided it. The Windows shim appends
 * to a file under ProgramData that the guest runner collects; a test records
 * them. */
#ifndef AV_ICD_LOG_SINK_HPP
#define AV_ICD_LOG_SINK_HPP

namespace av::icd {

class LogSink {
public:
    LogSink(const LogSink &) = delete;
    LogSink &operator=(const LogSink &) = delete;

    /* One line, NUL-terminated, without its newline. */
    virtual void write(const char *line) = 0;

protected:
    LogSink() = default;
    ~LogSink() = default;
};

}  // namespace av::icd

#endif  // AV_ICD_LOG_SINK_HPP
