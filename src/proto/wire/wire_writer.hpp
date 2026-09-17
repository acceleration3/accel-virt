/* Writes little-endian fields into a destination of fixed capacity. A field
 * that does not fit is not written at all and latches `too_large`; later writes
 * are ignored, so an encoder writes every field and checks status() once. */
#ifndef AV_WIRE_WRITER_HPP
#define AV_WIRE_WRITER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct WireWriterProbe;

class WireWriter {
public:
    /* A null pointer has no capacity, whatever `capacity` says. */
    WireWriter(uint8_t *bytes, size_t capacity);

    void write_u8(uint8_t value);
    void write_u16(uint16_t value);
    void write_u32(uint32_t value);
    void write_u64(uint64_t value);
    void copy_bytes(const uint8_t *source, size_t count);
    void write_zeros(size_t count);

    /* What the generated serializers call: a field of the registry's own type,
     * a run of bytes, and a varint for a length that is usually small. A field
     * that does not fit writes nothing and latches, as every other write does
     * (feature 063). */
    void write(const void *source, size_t count);
    template <typename T>
    void write_pod(const T &value)
    {
        static_assert(__is_trivially_copyable(T), "a field on the wire is plain data");
        write(&value, sizeof(T));
    }
    /* Seven bits a byte, low group first. */
    template <typename T>
    void write_varint(T value)
    {
        write_varint_bits(static_cast<uint64_t>(value));
    }

    Status status() const;
    size_t offset() const;
    /* Every byte the answer asked for, whether or not it fitted. An encoder
     * writes every field and checks status() once, so this is what a refused
     * answer has to report back: the device tells the guest the length it
     * needs, and the guest allocates and asks again (feature 042). */
    size_t wanted() const;

    bool invariants_hold() const;

private:
    void     write_varint_bits(uint64_t value);
    uint8_t *claim(size_t count);
    void     write_little_endian(uint64_t value, size_t count);

    uint8_t *bytes_;
    size_t   capacity_;
    size_t   offset_;
    size_t   wanted_;
    Status   status_;

    friend struct WireWriterProbe;
};

}  // namespace av

#endif  // AV_WIRE_WRITER_HPP
