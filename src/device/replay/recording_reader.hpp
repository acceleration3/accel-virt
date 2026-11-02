#ifndef AV_RECORDING_READER_HPP
#define AV_RECORDING_READER_HPP

#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/recording_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct RecordingReaderProbe;

class RecordingReader {
public:
    RecordingReader(const uint8_t *bytes, size_t length);

    [[nodiscard]] Status  open();
    bool                  opened() const;
    const RecordingHeader &header() const;
    [[nodiscard]] Status  next(RecordHeader &record, const uint8_t *&payload);

    uint64_t    offset() const;
    uint64_t    records() const;
    bool        damaged() const;
    uint64_t    damage_offset() const;
    const char *damage() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status damaged_by(const char *reason);

    const uint8_t *bytes_;
    size_t         length_;
    size_t         offset_;
    RecordingHeader header_;
    size_t         damage_offset_;
    const char    *damage_;
    uint64_t       records_;
    bool           opened_;

    friend struct RecordingReaderProbe;
};

}

#endif
