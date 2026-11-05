#ifndef AV_GLICD_BUFFER_CLEAR_PATTERN_HPP
#define AV_GLICD_BUFFER_CLEAR_PATTERN_HPP

#include <stdint.h>

namespace av::glicd {

class BufferClearPattern {
public:
    static constexpr uint32_t most_bytes = 16u;
    enum : uint32_t {
        gl_no_error = 0u, gl_invalid_enum = 0x0500u, gl_invalid_operation = 0x0502u,
    };

    BufferClearPattern();

    uint32_t convert(uint32_t internal_format, uint32_t format, uint32_t type, const void *data);
    uint32_t size() const;
    void     fill(uint8_t *destination, uint64_t bytes) const;

    bool invariants_hold() const;

private:
    enum class Numeric : uint8_t { unorm, floating, signed_integer, unsigned_integer };
    enum class Source : uint8_t { unsigned_integer, signed_integer, floating, half };

    struct Internal {
        uint32_t format_;
        uint8_t  components_;
        uint8_t  bytes_;
        Numeric  numeric_;
    };

    struct Layout {
        uint32_t format_;
        uint8_t  components_;
        bool     integer_;
        uint8_t  channels_[4];
    };

    struct Kind {
        uint32_t type_;
        uint8_t  bytes_;
        Source   source_;
    };

    static constexpr uint32_t internal_rows = 33u;
    static constexpr uint32_t layout_rows = 10u;
    static constexpr uint32_t kind_rows = 8u;
    static constexpr Internal internals_[internal_rows] = {
        {0x8229u, 1u, 1u, Numeric::unorm},          {0x822Au, 1u, 2u, Numeric::unorm},
        {0x822Du, 1u, 2u, Numeric::floating},       {0x822Eu, 1u, 4u, Numeric::floating},
        {0x8231u, 1u, 1u, Numeric::signed_integer}, {0x8233u, 1u, 2u, Numeric::signed_integer},
        {0x8235u, 1u, 4u, Numeric::signed_integer}, {0x8232u, 1u, 1u, Numeric::unsigned_integer},
        {0x8234u, 1u, 2u, Numeric::unsigned_integer}, {0x8236u, 1u, 4u, Numeric::unsigned_integer},
        {0x822Bu, 2u, 1u, Numeric::unorm},          {0x822Cu, 2u, 2u, Numeric::unorm},
        {0x822Fu, 2u, 2u, Numeric::floating},       {0x8230u, 2u, 4u, Numeric::floating},
        {0x8237u, 2u, 1u, Numeric::signed_integer}, {0x8239u, 2u, 2u, Numeric::signed_integer},
        {0x823Bu, 2u, 4u, Numeric::signed_integer}, {0x8238u, 2u, 1u, Numeric::unsigned_integer},
        {0x823Au, 2u, 2u, Numeric::unsigned_integer}, {0x823Cu, 2u, 4u, Numeric::unsigned_integer},
        {0x8815u, 3u, 4u, Numeric::floating},       {0x8D83u, 3u, 4u, Numeric::signed_integer},
        {0x8D71u, 3u, 4u, Numeric::unsigned_integer}, {0x8058u, 4u, 1u, Numeric::unorm},
        {0x805Bu, 4u, 2u, Numeric::unorm},          {0x881Au, 4u, 2u, Numeric::floating},
        {0x8814u, 4u, 4u, Numeric::floating},       {0x8D8Eu, 4u, 1u, Numeric::signed_integer},
        {0x8D88u, 4u, 2u, Numeric::signed_integer}, {0x8D82u, 4u, 4u, Numeric::signed_integer},
        {0x8D7Cu, 4u, 1u, Numeric::unsigned_integer}, {0x8D76u, 4u, 2u, Numeric::unsigned_integer},
        {0x8D70u, 4u, 4u, Numeric::unsigned_integer},
    };
    static constexpr Layout layouts_[layout_rows] = {
        {0x1903u, 1u, false, {0u, 0u, 0u, 0u}}, {0x8227u, 2u, false, {0u, 1u, 0u, 0u}},
        {0x1907u, 3u, false, {0u, 1u, 2u, 0u}}, {0x1908u, 4u, false, {0u, 1u, 2u, 3u}},
        {0x80E0u, 3u, false, {2u, 1u, 0u, 0u}}, {0x80E1u, 4u, false, {2u, 1u, 0u, 3u}},
        {0x8D94u, 1u, true, {0u, 0u, 0u, 0u}},  {0x8228u, 2u, true, {0u, 1u, 0u, 0u}},
        {0x8D98u, 3u, true, {0u, 1u, 2u, 0u}},  {0x8D99u, 4u, true, {0u, 1u, 2u, 3u}},
    };
    static constexpr Kind kinds_[kind_rows] = {
        {0x1401u, 1u, Source::unsigned_integer}, {0x1400u, 1u, Source::signed_integer},
        {0x1403u, 2u, Source::unsigned_integer}, {0x1402u, 2u, Source::signed_integer},
        {0x1405u, 4u, Source::unsigned_integer}, {0x1404u, 4u, Source::signed_integer},
        {0x1406u, 4u, Source::floating},         {0x140Bu, 2u, Source::half},
    };

    const Internal *internal_of(uint32_t format) const;
    const Layout   *layout_of(uint32_t format) const;
    const Kind     *kind_of(uint32_t type) const;
    int64_t         integer_at(const uint8_t *element, const Kind &kind) const;
    float           real_at(const uint8_t *element, const Kind &kind) const;
    void            store_normalized(uint32_t component, float value);
    void            store_integer(uint32_t component, int64_t value);
    void            store_floating(uint32_t component, float value);

    uint8_t         bytes_[most_bytes];
    uint32_t        size_;
    const Internal *internal_;
};

}

#endif
