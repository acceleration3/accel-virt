/* Which record kinds a recording keeps: the device's `record-kinds` property,
 * as the mask the file header carries. */
#ifndef AV_RECORD_KINDS_HPP
#define AV_RECORD_KINDS_HPP

#include "core/support/status.hpp"
#include "device/recording/record_kind.hpp"

#include <stdint.h>

namespace av {

class RecordKinds {
public:
    /* Kinds 1 to 7. */
    static constexpr uint32_t every_kind = 0x7Fu;
    /* A blob's bytes are the one kind whose size the wire does not bound, so
     * they are kept only when asked for. */
    static constexpr uint32_t by_default = every_kind & ~(1u << 6);

    RecordKinds();

    bool     keeps(RecordKind kind) const;
    uint32_t mask() const;
    /* The comma-separated kind names, or "all"; an empty list is the default.
     * invalid_parameter for a name no kind has, leaving the mask alone. */
    [[nodiscard]] Status parse(const char *list);

    bool invariants_hold() const;

private:
    /* A property longer than this is a mistake, not a list. */
    static constexpr uint32_t longest_list = 256u;

    /* What the name of `length` bytes at `text` turns on, or 0. */
    uint32_t bit_of(const char *text, uint32_t length) const;

    uint32_t mask_;
};

}  // namespace av

#endif  // AV_RECORD_KINDS_HPP
