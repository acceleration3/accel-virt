/* The calls a driver refused an application, each written to the driver's
 * log the first time: an entry point it does not serve, an entry point with
 * each failure it answered, and a call it could not send. A runtime passes a
 * refusal on as the API's error and says nothing of where it came from, and
 * an application that retries in a loop - or makes the same call every frame
 * - would otherwise write a line per call. Every user-mode driver refuses
 * through one, over the KeyedLog its root owns. */
#ifndef AV_ICD_REFUSED_CALLS_HPP
#define AV_ICD_REFUSED_CALLS_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {
class KeyedLog;
}  // namespace av

namespace av::icd {

class RefusedCalls {
public:
    explicit RefusedCalls(KeyedLog &lines);
    RefusedCalls(const RefusedCalls &) = delete;
    RefusedCalls &operator=(const RefusedCalls &) = delete;

    /* `entry` is a literal - a slot's or an entry point's name - so one entry
     * is one pointer. Null is said nowhere. */
    void unserved(const char *entry);
    /* `code`, which the caller judged a failure, as the API spells it: an
     * HRESULT's bit pattern in hex, an API's error number in decimal. */
    void failed(const char *entry, int64_t code);
    /* A call whose encoding overflowed at byte `written` of what `api`
     * (a literal) sends, so it never reached the host. */
    void dropped(const char *api, const char *entry, size_t written);

private:
    /* An HRESULT's failures are 0x8xxxxxxx: as a signed 32-bit value, below
     * this. Every API's own error numbers are well above it. */
    static constexpr int64_t hresult_floor = -0x70000000ll;
    /* The code dropped() keys its entries by: no API answers it. */
    static constexpr int64_t dropped_code = INT64_MIN;

    KeyedLog &lines_;
};

}  // namespace av::icd

#endif  // AV_ICD_REFUSED_CALLS_HPP
