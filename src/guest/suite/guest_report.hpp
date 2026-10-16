/* The in-guest suite's verdict as tools/guest/run_guest_suite.py reads it: one
 * line per step, `ok scenario.step`, `ok scenario.step N` for a step that
 * measured something, or
 * `FAIL scenario.step status=N code=0xXXXXXXXX`, then
 * `<program>: P passed, F failed`, the program avtest unless named. The text
 * grows with the steps - a suite replaying an application's trace runs
 * thousands - so the summary is never cut off; a line there is no memory to
 * keep is lost, the counts are not, and a report without its summary line
 * reads as a failure. */
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
    /* The longest line kept whole: scenario and step names are literals, and
     * a note is cut at note_bytes. */
    static constexpr size_t line_bytes = 512u;

    explicit GuestReport(BlockSource &memory);
    GuestReport(BlockSource &memory, const char *program);
    GuestReport(const GuestReport &) = delete;
    GuestReport &operator=(const GuestReport &) = delete;

    /* Passes when `status` is ok. */
    void check(const char *scenario, const char *step, Status status, uint32_t code);
    /* Passes when `status` is not ok: something the driver must refuse. */
    void check_refused(const char *scenario, const char *step, Status status, uint32_t code);
    /* A step that measured something: the number is on the line, passed or
     * not, because it is what the run was for. */
    void measure(const char *scenario, const char *step, Status status, uint32_t measurement);
    /* `note scenario.key text`: something a step learned that is not a
     * verdict, up to its first line break and note_bytes bytes; `(none)` for
     * null. */
    void note(const char *scenario, const char *key, const char *text);
    void finish();

    const char *text() const;
    size_t      length() const;
    uint32_t    passed() const;
    uint32_t    failed() const;
    /* `report/0` with the counts and the step recorded last - where a
     * program that stopped answering got to (docs/trace.md). */
    void        report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    void line(const char *scenario, const char *step, bool passed, Status status, uint32_t code);
    /* `written` added to the text, which stays NUL-terminated. */
    void keep(const TextBuffer &written);

    const char         *program_;
    /* The text and its terminator; empty until the first line. */
    GrowableArray<char> text_;
    uint32_t   passed_;
    uint32_t   failed_;
    /* The step checked last; the state service reads these from another
     * thread, so each is one pointer to a literal, never a buffer. */
    const char *last_scenario_;
    const char *last_step_;
    bool        finished_;

    friend struct GuestReportProbe;
};

}  // namespace av

#endif  // AV_GUEST_REPORT_HPP
