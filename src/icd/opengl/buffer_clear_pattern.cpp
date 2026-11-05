#include "icd/opengl/buffer_clear_pattern.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/half_float.hpp"

namespace av::glicd {

BufferClearPattern::BufferClearPattern() : bytes_{}, size_(0u), internal_(nullptr)
{
    AV_SCOPE("BufferClearPattern::BufferClearPattern");
}

const BufferClearPattern::Internal *BufferClearPattern::internal_of(uint32_t format) const
{
    AV_SCOPE("BufferClearPattern::internal_of");
    for (const Internal &row : internals_)
        if (row.format_ == format) return &row;
    return nullptr;
}

const BufferClearPattern::Layout *BufferClearPattern::layout_of(uint32_t format) const
{
    AV_SCOPE("BufferClearPattern::layout_of");
    for (const Layout &row : layouts_)
        if (row.format_ == format) return &row;
    return nullptr;
}

const BufferClearPattern::Kind *BufferClearPattern::kind_of(uint32_t type) const
{
    AV_SCOPE("BufferClearPattern::kind_of");
    for (const Kind &row : kinds_)
        if (row.type_ == type) return &row;
    return nullptr;
}

uint32_t BufferClearPattern::convert(uint32_t internal_format, uint32_t format, uint32_t type, const void *data)
{
    AV_SCOPE("BufferClearPattern::convert");
    __builtin_memset(bytes_, 0, sizeof bytes_);
    size_ = 0u;
    internal_ = internal_of(internal_format);
    const Layout *const layout = layout_of(format);
    const Kind *const   kind = kind_of(type);
    if (internal_ == nullptr || layout == nullptr || kind == nullptr) return gl_invalid_enum;
    const bool integer_target =
        internal_->numeric_ == Numeric::signed_integer || internal_->numeric_ == Numeric::unsigned_integer;
    if (layout->integer_ != integer_target) return gl_invalid_operation;
    if (integer_target && (kind->source_ == Source::floating || kind->source_ == Source::half))
        return gl_invalid_operation;
    size_ = static_cast<uint32_t>(internal_->components_) * internal_->bytes_;
    if (data == nullptr) return gl_no_error;
    int64_t       whole[4] = {0, 0, 0, 1};
    float         real[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    const uint8_t *const source = static_cast<const uint8_t *>(data);
    for (uint32_t position = 0u; position < layout->components_; position++) {
        const uint8_t *const element = source + position * kind->bytes_;
        whole[layout->channels_[position]] = integer_at(element, *kind);
        real[layout->channels_[position]] = real_at(element, *kind);
    }
    for (uint32_t component = 0u; component < internal_->components_; component++) {
        if (internal_->numeric_ == Numeric::unorm) store_normalized(component, real[component]);
        else if (internal_->numeric_ == Numeric::floating) store_floating(component, real[component]);
        else store_integer(component, whole[component]);
    }
    return gl_no_error;
}

int64_t BufferClearPattern::integer_at(const uint8_t *element, const Kind &kind) const
{
    AV_SCOPE("BufferClearPattern::integer_at");
    if (kind.source_ == Source::unsigned_integer) {
        if (kind.bytes_ == 1u) return element[0];
        if (kind.bytes_ == 2u) {
            uint16_t value = 0u;
            __builtin_memcpy(&value, element, sizeof value);
            return value;
        }
        uint32_t value = 0u;
        __builtin_memcpy(&value, element, sizeof value);
        return value;
    }
    if (kind.source_ != Source::signed_integer) return 0;
    if (kind.bytes_ == 1u) return static_cast<int8_t>(element[0]);
    if (kind.bytes_ == 2u) {
        int16_t value = 0;
        __builtin_memcpy(&value, element, sizeof value);
        return value;
    }
    int32_t value = 0;
    __builtin_memcpy(&value, element, sizeof value);
    return value;
}

float BufferClearPattern::real_at(const uint8_t *element, const Kind &kind) const
{
    AV_SCOPE("BufferClearPattern::real_at");
    if (kind.source_ == Source::floating) {
        float value = 0.0f;
        __builtin_memcpy(&value, element, sizeof value);
        return value;
    }
    if (kind.source_ == Source::half) {
        uint16_t bits = 0u;
        __builtin_memcpy(&bits, element, sizeof bits);
        return HalfFloat(bits).value();
    }
    const int64_t whole = integer_at(element, kind);
    const double  unit = kind.source_ == Source::unsigned_integer
                             ? static_cast<double>((uint64_t{1} << (8u * kind.bytes_)) - 1u)
                             : static_cast<double>((uint64_t{1} << (8u * kind.bytes_ - 1u)) - 1u);
    const float scaled = static_cast<float>(static_cast<double>(whole) / unit);
    return scaled < -1.0f ? -1.0f : scaled;
}

void BufferClearPattern::store_normalized(uint32_t component, float value)
{
    AV_SCOPE("BufferClearPattern::store_normalized");
    const float clamped = value != value || value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    const double unit = static_cast<double>((uint64_t{1} << (8u * internal_->bytes_)) - 1u);
    store_integer(component, static_cast<int64_t>(static_cast<double>(clamped) * unit + 0.5));
}

void BufferClearPattern::store_floating(uint32_t component, float value)
{
    AV_SCOPE("BufferClearPattern::store_floating");
    uint8_t *const at = bytes_ + component * internal_->bytes_;
    if (internal_->bytes_ == 2u) {
        const uint16_t bits = HalfFloat(value).bits();
        __builtin_memcpy(at, &bits, sizeof bits);
        return;
    }
    __builtin_memcpy(at, &value, sizeof value);
}

void BufferClearPattern::store_integer(uint32_t component, int64_t value)
{
    AV_SCOPE("BufferClearPattern::store_integer");
    const uint32_t bits = 8u * internal_->bytes_;
    const bool     is_signed = internal_->numeric_ == Numeric::signed_integer;
    const int64_t  lowest = is_signed ? -(int64_t{1} << (bits - 1u)) : 0;
    const int64_t  highest = is_signed ? (int64_t{1} << (bits - 1u)) - 1 : (int64_t{1} << bits) - 1;
    const int64_t  clamped = value < lowest ? lowest : (value > highest ? highest : value);
    const uint64_t pattern = static_cast<uint64_t>(clamped);
    uint8_t *const at = bytes_ + component * internal_->bytes_;
    for (uint32_t byte = 0u; byte < internal_->bytes_; byte++) at[byte] = static_cast<uint8_t>(pattern >> (8u * byte));
}

uint32_t BufferClearPattern::size() const
{
    AV_SCOPE("BufferClearPattern::size");
    return size_;
}

void BufferClearPattern::fill(uint8_t *destination, uint64_t bytes) const
{
    AV_SCOPE("BufferClearPattern::fill");
    if (size_ == 0u) return;
    for (uint64_t at = 0u; at + size_ <= bytes; at += size_) __builtin_memcpy(destination + at, bytes_, size_);
}

bool BufferClearPattern::invariants_hold() const
{
    AV_SCOPE("BufferClearPattern::invariants_hold");
    return size_ <= most_bytes &&
           (size_ == 0u || (internal_ != nullptr && size_ == uint32_t{internal_->components_} * internal_->bytes_));
}

}
