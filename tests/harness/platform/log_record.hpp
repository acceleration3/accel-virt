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
    const char   *last() const;

private:
    std::uint32_t count_;
    char          last_[capacity];
};

}

#endif
