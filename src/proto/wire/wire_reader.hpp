#ifndef AV_WIRE_READER_HPP
#define AV_WIRE_READER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct WireReaderProbe;

class WireReader {
public:
    WireReader(const uint8_t *bytes, size_t length);

    uint8_t  read_u8();
    uint16_t read_u16();
    uint32_t read_u32();
    uint64_t read_u64();

    void copy_bytes(uint8_t *destination, size_t count);

    [[nodiscard]] bool read(void *destination, size_t count);
    template <typename T>
    [[nodiscard]] bool read_pod(T &out)
    {
        static_assert(__is_trivially_copyable(T), "a field on the wire is plain data");
        return read(&out, sizeof(T));
    }
    template <typename T>
    [[nodiscard]] bool read_varint(T &out)
    {
        uint64_t value = 0u;
        if (!read_varint_bits(value, sizeof(T) * 8u)) return false;
        out = static_cast<T>(value);
        return true;
    }
    void expect_zero(size_t count);
    void skip(size_t count);

    Status status() const;
    size_t offset() const;
    size_t remaining() const;

    bool invariants_hold() const;

private:
    static constexpr size_t max_varint_bits = 70u;

    [[nodiscard]] bool read_varint_bits(uint64_t &value, size_t bits);
    const uint8_t *take(size_t count);
    uint64_t       read_little_endian(size_t count);

    const uint8_t *bytes_;
    size_t         length_;
    size_t         offset_;
    Status         status_;

    friend struct WireReaderProbe;
};

}

#endif
