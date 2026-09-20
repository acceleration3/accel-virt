/* One non-blocking read the application has not been given the bytes of yet.
 *
 * clEnqueueReadBuffer with blocking CL_FALSE must return as soon as the read
 * is queued, and the application's memory is undefined until the event says
 * otherwise - which means the bytes cannot be fetched at the call. What the
 * ICD keeps until then is this: where in the buffer, how many bytes, and the
 * application's destination, against the event that says when they are ready.
 *
 * The destination is the application's own pointer, held across calls. That is
 * what the API promises about it - the memory must stay valid and must not be
 * touched until the event completes - so holding it is not a lifetime this ICD
 * invented. */
#ifndef AV_CLICD_CL_PENDING_READ_HPP
#define AV_CLICD_CL_PENDING_READ_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

struct ClPendingReadProbe;

class ClPendingRead {
public:
    ClPendingRead();

    /* Takes a row. False for a read of nothing or with nowhere to put it,
     * which is the caller's mistake and not something to remember. */
    /* `ours` is an event the ICD made for itself because the application
     * asked for none: nobody else will release it, so delivery does. */
    [[nodiscard]] bool take(uint64_t event, uint64_t queue, uint64_t buffer, size_t offset,
                            size_t bytes, void *into, bool ours);
    void               give_up();
    bool               held() const;

    uint64_t event() const;
    uint64_t queue() const;
    uint64_t buffer() const;
    size_t   offset() const;
    size_t   bytes() const;
    void    *into() const;
    bool     ours() const;

    bool invariants_hold() const;

private:
    friend struct ClPendingReadProbe;

    void    *into_;
    uint64_t event_;
    uint64_t queue_;
    uint64_t buffer_;
    size_t   offset_;
    size_t   bytes_;
    bool     ours_;
    bool     held_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_PENDING_READ_HPP
