#ifndef AV_RECORD_KINDS_HPP
#define AV_RECORD_KINDS_HPP

#include "core/support/status.hpp"
#include "device/recording/record_kind.hpp"

#include <stdint.h>

namespace av {

struct RecordKindsProbe;

class RecordKinds {
public:
    static constexpr uint32_t every_kind = 0x7Fu;
    static constexpr uint32_t by_default = every_kind & ~(1u << 6);

    RecordKinds();

    bool     keeps(RecordKind kind) const;
    uint32_t mask() const;
    [[nodiscard]] Status parse(const char *list);

    bool invariants_hold() const;

private:
    static constexpr uint32_t longest_list = 256u;

    uint32_t bit_of(const char *text, uint32_t length) const;

    uint32_t mask_;

    friend struct RecordKindsProbe;
};

}

#endif
