/* The in-guest suite's verdict as tools/guest/run_guest_suite.py reads it: one
 * line per step, `ok scenario.step`, `ok scenario.step N` for a step that
 * measured something, or
 * `FAIL scenario.step status=N code=0xXXXXXXXX`, then
 * `<program>: P passed, F failed`, the program rvktest unless named. Text past the capacity is cut; the counts
 * are not, and a report without its summary line reads as a failure. */
#ifndef AV_GUEST_REPORT_HPP
#define AV_GUEST_REPORT_HPP

#include "core/support/status.hpp"
#include "core/support/text_buffer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class StateWriter;

class GuestReport {
public:
    static constexpr size_t capacity = 16384u;

    GuestReport();
    explicit GuestReport(const char *program);
    GuestReport(const GuestReport &) = delete;
    GuestReport &operator=(const GuestReport &) = delete;

    /* Passes when `status` is ok. */
    void check(const char *scenario, const char *step, Status status, uint32_t code);
    /* Passes when `status` is not ok: something the driver must refuse. */
    void check_refused(const char *scenario, const char *step, Status status, uint32_t code);
    /* A step that measured something: the number is on the line, passed or
     * not, because it is what the run was for. */
    void measure(const char *scenario, const char *step, Status status, uint32_t measurement);
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

    const char *program_;
    char       text_[capacity];
    TextBuffer buffer_;
    uint32_t   passed_;
    uint32_t   failed_;
    /* The step checked last; the state service reads these from another
     * thread, so each is one pointer to a literal, never a buffer. */
    const char *last_scenario_;
    const char *last_step_;
    bool        finished_;
};

}  // namespace av

#endif  // AV_GUEST_REPORT_HPP
