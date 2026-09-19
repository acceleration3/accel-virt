/* Walks a wire recording's records (docs/protocol.md). Every byte of a
 * recording is hostile: it was written by a device decoding a guest's bytes,
 * a run may have been cut short by the crash it was recording, and a file on
 * disk is anyone's to edit. So nothing here is trusted - each record is framed
 * against the bytes the reader owns, and the first thing that does not frame
 * stops the walk and is reported with its offset, leaving every whole record
 * before it already answered. */
#ifndef AV_RECORDING_READER_HPP
#define AV_RECORDING_READER_HPP

#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/recording_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RecordingReader {
public:
    /* The bytes stay the caller's and must outlive the reader; a null pointer
     * is an empty recording, whatever `length` says. */
    RecordingReader(const uint8_t *bytes, size_t length);

    /* The file header. malformed for bytes no writer produced, not_supported
     * for a major this reader does not know, invalid_state once open; either
     * failure is the recording's damage at offset 0. */
    [[nodiscard]] Status  open();
    bool                  opened() const;
    const RecordingHeader &header() const;
    /* The next record and the `record.length()` bytes at `payload`: ok, or
     * not_found at a clean end, or malformed with the damage set. Once
     * damaged, every later call answers malformed and reads nothing.
     * invalid_state before open. */
    [[nodiscard]] Status  next(RecordHeader &record, const uint8_t *&payload);

    /* Where the walk is, which is the end of the last record answered. */
    uint64_t    offset() const;
    /* Records answered so far. */
    uint64_t    records() const;
    bool        damaged() const;
    /* Where the damage starts; meaningless while undamaged. */
    uint64_t    damage_offset() const;
    /* Why, in a few words; the empty string while undamaged. */
    const char *damage() const;

    bool invariants_hold() const;

private:
    /* Latches the damage at the walk's offset and answers malformed. */
    [[nodiscard]] Status damaged_by(const char *reason);

    const uint8_t *bytes_;
    size_t         length_;
    size_t         offset_;
    RecordingHeader header_;
    size_t         damage_offset_;
    const char    *damage_;
    uint64_t       records_;
    bool           opened_;
};

}  // namespace av

#endif  // AV_RECORDING_READER_HPP
