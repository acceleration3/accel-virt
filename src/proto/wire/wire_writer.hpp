#ifndef AV_WIRE_WRITER_HPP
#define AV_WIRE_WRITER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireSpill;
struct WireWriterProbe;

class WireWriter {
public:
    WireWriter(uint8_t *bytes, size_t capacity);
    WireWriter(uint8_t *bytes, size_t capacity, WireSpill &spill);

    void write_u8(uint8_t value);
    void write_u16(uint16_t value);
    void write_u32(uint32_t value);
    void write_u64(uint64_t value);
    void copy_bytes(const uint8_t *source, size_t count);
    void write_zeros(size_t count);

    void write(const void *source, size_t count);
    template <typename T>
    void write_pod(const T &value)
    {
        static_assert(__is_trivially_copyable(T), "a field on the wire is plain data");
        write(&value, sizeof(T));
    }
    template <typename T>
    void write_varint(T value)
    {
        write_varint_bits(static_cast<uint64_t>(value));
    }

    Status status() const;
    size_t offset() const;
    const uint8_t *bytes() const;
    bool           spilled() const;
    size_t wanted() const;

    bool invariants_hold() const;

private:
    void     write_varint_bits(uint64_t value);
    uint8_t *claim(size_t count);
    bool     grow(size_t count);
    void     write_little_endian(uint64_t value, size_t count);

    uint8_t   *bytes_;
    size_t     capacity_;
    size_t     offset_;
    size_t     wanted_;
    Status     status_;
    WireSpill *spill_;
    bool       spilled_;

    friend struct WireWriterProbe;
};

}

#endif
