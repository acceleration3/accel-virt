/* The guest memory a context's commands name by offset - its blob arena, as
 * the host maps it - which every decoder resolves such an offset in, bounds
 * checked.
 *
 * Every API's generated decoder takes one at construction (its
 * ServerDispatch, OpenGL's table), so the one rule for an offset off the wire
 * is here rather than in an override per backend. OpenGL's decoder asks at
 * more than a thousand sites: it is the bulk-data path, not a corner case. */
#ifndef AV_GUEST_BYTES_HPP
#define AV_GUEST_BYTES_HPP

#include <stdint.h>

namespace av {

class GuestBytes {
public:
    GuestBytes();
    GuestBytes(uint8_t *base, uint64_t length);
    GuestBytes(const GuestBytes &) = delete;
    GuestBytes &operator=(const GuestBytes &) = delete;

    /* Offsets resolved in `length` bytes at `base` from now on. False, with
     * nothing changed, while bound, and for no memory. */
    [[nodiscard]] bool bind(uint8_t *base, uint64_t length);
    void               unbind();
    /* Whether the bytes are the `length` at `base`. */
    bool               over(const uint8_t *base) const;

    /* `bytes` at `offset`, or null: unbound, an offset past the end, a length
     * past the end, and a sum that would wrap. */
    uint8_t *at(uint64_t offset, uint64_t bytes) const;
    /* Whether `bytes` points inside them: memory a decoder resolved through
     * `at`, rather than a copy of its own. */
    bool     holds(const void *bytes) const;
    uint64_t length() const;

    bool invariants_hold() const;

private:
    uint8_t *base_;
    uint64_t length_;
};

}  // namespace av

#endif  // AV_GUEST_BYTES_HPP
