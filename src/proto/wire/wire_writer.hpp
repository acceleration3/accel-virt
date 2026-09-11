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

    Status status() const;
    size_t offset() const;

    bool invariants_hold() const;

private:
    uint8_t *claim(size_t count);
    void     write_little_endian(uint64_t value, size_t count);

    uint8_t *bytes_;
    size_t   capacity_;
    size_t   offset_;
    Status   status_;

    friend struct WireWriterProbe;
};

}  // namespace av

#endif  // AV_WIRE_WRITER_HPP
