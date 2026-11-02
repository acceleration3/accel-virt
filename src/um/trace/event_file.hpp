#ifndef AV_UM_EVENT_FILE_HPP
#define AV_UM_EVENT_FILE_HPP

#include "core/support/status.hpp"
#include "um/common/diagnostics_directory.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class TraceTable;

class EventFile {
public:
    EventFile();
    EventFile(const EventFile &) = delete;
    EventFile &operator=(const EventFile &) = delete;

    bool                 wanted() const;
    uint32_t             events_per_slot() const;
    uint32_t             depth() const;
    void                 only(TraceTable &table) const;
    [[nodiscard]] Status open(const char *module, uint32_t process, size_t bytes);
    uint8_t             *region() const;
    size_t               bytes() const;

private:
    static constexpr uint32_t flag_capacity = 1024u;

    void        read_flag(char *text, uint32_t capacity) const;
    const char *field(const char *text, uint32_t index) const;
    uint32_t    number(uint32_t index, uint32_t fallback) const;

    DiagnosticsDirectory directory_;
    uint8_t             *region_;
    size_t               bytes_;
};

}

#endif
