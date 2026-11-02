#include "device/recording/record_kinds.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {
class KindName {
public:
    constexpr KindName(const char *text, uint32_t bit) : text_(text), bit_(bit) {}

    constexpr uint32_t bit_for(const char *text, uint32_t length) const
    {
        uint32_t index = 0u;
        for (; index < length; index++) {
            if (text_[index] != text[index]) return 0u;
        }
        return text_[index] == '\0' ? bit_ : 0u;
    }

private:
    const char *text_;
    uint32_t    bit_;
};

constexpr KindName kind_names[] = {
    KindName("setup", 1u << 0),      KindName("batch", 1u << 1), KindName("reply", 1u << 2),
    KindName("fence", 1u << 3),      KindName("fault", 1u << 4), KindName("blob", 1u << 5),
    KindName("blob_bytes", 1u << 6), KindName("all", RecordKinds::every_kind),
};

constexpr uint32_t name_count = sizeof kind_names / sizeof kind_names[0];

}

RecordKinds::RecordKinds() : mask_(by_default)
{
    AV_SCOPE("RecordKinds::RecordKinds");
}

bool RecordKinds::keeps(RecordKind kind) const
{
    AV_SCOPE("RecordKinds::keeps");
    const uint32_t bit = 1u << (static_cast<uint32_t>(kind) - 1u);
    return (mask_ & bit) != 0u;
}

uint32_t RecordKinds::mask() const
{
    AV_SCOPE("RecordKinds::mask");
    return mask_;
}

uint32_t RecordKinds::bit_of(const char *text, uint32_t length) const
{
    AV_SCOPE("RecordKinds::bit_of");
    for (uint32_t index = 0u; index < name_count; index++) {
        const uint32_t bit = kind_names[index].bit_for(text, length);
        if (bit != 0u) return bit;
    }
    return 0u;
}

Status RecordKinds::parse(const char *list)
{
    AV_SCOPE("RecordKinds::parse");
    if (list == nullptr) {
        mask_ = by_default;
        return Status::ok;
    }
    uint32_t found = 0u;
    uint32_t start = 0u;
    for (uint32_t index = 0u;; index++) {
        if (index >= longest_list) return Status::invalid_parameter;
        if (list[index] != ',' && list[index] != '\0') continue;
        const uint32_t length = index - start;
        if (length != 0u) {
            const uint32_t bit = bit_of(list + start, length);
            if (bit == 0u) return Status::invalid_parameter;
            found |= bit;
        }
        start = index + 1u;
        if (list[index] == '\0') break;
    }
    mask_ = found == 0u ? by_default : found;
    return Status::ok;
}

bool RecordKinds::invariants_hold() const
{
    AV_SCOPE("RecordKinds::invariants_hold");
    return mask_ != 0u && (mask_ & ~every_kind) == 0u;
}

}
