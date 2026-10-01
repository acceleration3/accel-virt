/* The file a Windows process's timeline events go to while Perfetto records:
 * C:\ProgramData\av\events-<module>-<pid>.bin, mapped, so the host pulls it
 * through the guest agent while the process still writes. Recording is on
 * while C:\ProgramData\av\perfetto exists when the process starts. */
#ifndef AV_UM_EVENT_FILE_HPP
#define AV_UM_EVENT_FILE_HPP

#include "core/support/status.hpp"

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
    /* How many events a thread's ring holds: the flag file's number, when it
     * names one, or a second or so of a busy thread. */
    uint32_t             events_per_slot() const;
    /* How deep a scope is still recorded: the flag file's second number, when
     * it names one, or the outer calls only (TraceTable::limit_depth). */
    uint32_t             depth() const;
    /* The flag file's third field, when it has one: the only scopes recorded
     * (TraceTable::only). */
    void                 only(TraceTable &table) const;
    [[nodiscard]] Status open(const char *module, uint32_t process, size_t bytes);
    uint8_t             *region() const;
    size_t               bytes() const;

private:
    static constexpr uint32_t flag_capacity = 1024u;

    void        read_flag(char *text, uint32_t capacity) const;
    const char *field(const char *text, uint32_t index) const;
    uint32_t    number(uint32_t index, uint32_t fallback) const;

    uint8_t *region_;
    size_t   bytes_;
};

}  // namespace av

#endif  // AV_UM_EVENT_FILE_HPP
