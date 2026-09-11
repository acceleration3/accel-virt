/* Reads little-endian fields from bytes this side did not write - a capset, a
 * ring header, a fence page. The first failed read latches `malformed`; later
 * reads return zero and do not advance, so a decoder reads every field and
 * checks status() once. */
#ifndef AV_WIRE_READER_HPP
#define AV_WIRE_READER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct WireReaderProbe;

class WireReader {
public:
    /* A null pointer is an empty buffer, whatever `length` says. */
    WireReader(const uint8_t *bytes, size_t length);

    uint8_t  read_u8();
    uint16_t read_u16();
    uint32_t read_u32();
    uint64_t read_u64();

    /* Zero-fills the destination when the bytes are not there. */
    void copy_bytes(uint8_t *destination, size_t count);
    /* Reserved fields: any non-zero byte is malformed. */
    void expect_zero(size_t count);

    Status status() const;
    size_t offset() const;
    /* Zero once a read has failed. */
    size_t remaining() const;

    bool invariants_hold() const;

private:
    /* The start of the next `count` bytes, or null after latching failure. */
    const uint8_t *take(size_t count);
    uint64_t       read_little_endian(size_t count);

    const uint8_t *bytes_;
    size_t         length_;
    size_t         offset_;
    Status         status_;

    friend struct WireReaderProbe;
};

}  // namespace av

#endif  // AV_WIRE_READER_HPP
