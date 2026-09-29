/* The calls an application was refused: an entry point this ICD does not
 * serve, or one that answered an error. Each entry point, with each error it
 * answered, is named in the log the first time. An application that only
 * reports "a problem" - Photoshop's Blur Gallery - otherwise leaves nothing to
 * say which call it was refused, and one that retries in a loop would
 * otherwise write a line per call. */
#ifndef AV_CLICD_CL_REFUSAL_LOG_HPP
#define AV_CLICD_CL_REFUSAL_LOG_HPP

#include <stdint.h>

namespace av::icd {
class LogSink;
class ThreadLock;
}  // namespace av::icd

namespace av::clicd {

class ClRefusalLog {
public:
    /* Room for every slot of cl_icd_dispatch this ICD leaves to the generated
     * refusals and a few errors from each entry point it serves. */
    static constexpr uint32_t max_named = 256u;

    ClRefusalLog(icd::LogSink &log, icd::ThreadLock &lock);
    ClRefusalLog(const ClRefusalLog &) = delete;
    ClRefusalLog &operator=(const ClRefusalLog &) = delete;

    /* `entry_point` is a literal, the generated refusal's or the hook's, so
     * one entry point is one pointer. */
    void     note(const char *entry_point);
    void     note_failure(const char *entry_point, int32_t error);
    uint32_t named() const;

private:
    /* Whether `entry_point` with `error` is new, and so to be written. */
    bool first(const char *entry_point, int32_t error);

    icd::LogSink    &log_;
    icd::ThreadLock &lock_;
    const char      *named_[max_named];
    int32_t          errors_[max_named];
    uint32_t         count_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_REFUSAL_LOG_HPP
