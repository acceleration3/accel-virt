#ifndef AV_GUEST_REPORT_HPP
#define AV_GUEST_REPORT_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;
class StateWriter;
class TextBuffer;
struct GuestReportProbe;

class GuestReport {
public:
    static constexpr size_t note_bytes = 160u;
    static constexpr size_t line_bytes = 512u;

    explicit GuestReport(BlockSource &memory);
    GuestReport(BlockSource &memory, const char *program);
    GuestReport(const GuestReport &) = delete;
    GuestReport &operator=(const GuestReport &) = delete;

    void check(const char *scenario, const char *step, Status status, uint32_t code);
    void check_refused(const char *scenario, const char *step, Status status, uint32_t code);
    void measure(const char *scenario, const char *step, Status status, uint32_t measurement);
    void note(const char *scenario, const char *key, const char *text);
    void finish();

    const char *text() const;
    size_t      length() const;
    uint32_t    passed() const;
    uint32_t    failed() const;
    void        report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    void line(const char *scenario, const char *step, bool passed, Status status, uint32_t code);
    void keep(const TextBuffer &written);

    const char         *program_;
    GrowableArray<char> text_;
    uint32_t   passed_;
    uint32_t   failed_;
    const char *last_scenario_;
    const char *last_step_;
    bool        finished_;

    friend struct GuestReportProbe;
};

}

#endif
