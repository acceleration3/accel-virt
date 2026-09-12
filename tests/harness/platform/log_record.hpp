/* What a platform logged, kept outside it. A refusal on a teardown path is
 * logged by an object that then destroys the platform - DriverImage::enter
 * releases everything when dxgkrnl refuses the registration - so a test that
 * means to assert the line needs somewhere for it to outlive the platform. */
#ifndef AV_LOG_RECORD_HPP
#define AV_LOG_RECORD_HPP

#include <cstdint>

namespace av {

class LogRecord {
public:
    static constexpr std::uint32_t capacity = 192u;

    LogRecord();
    LogRecord(const LogRecord &) = delete;
    LogRecord &operator=(const LogRecord &) = delete;

    void          write(const char *message);
    std::uint32_t count() const;
    /* The last message written, empty before the first, cut at the capacity. */
    const char   *last() const;

private:
    std::uint32_t count_;
    char          last_[capacity];
};

}  // namespace av

#endif  // AV_LOG_RECORD_HPP
