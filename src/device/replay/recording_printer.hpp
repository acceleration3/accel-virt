/* A recording as lines a person reads (contracts/avrecord.md). One line per
 * record and one indented line per command of a batch, answered one at a time
 * so a recording of any size costs one line's worth of memory; the damage the
 * reader found, if any, is the last line.
 *
 * It learns each context's sub-API from the setup record that bound it, which
 * is what lets `--api` keep a context's replies, fences and faults and not
 * only the batches whose commands carry an API in their ids - for every
 * context the recording names, however many the guest made while it ran. */
#ifndef AV_RECORDING_PRINTER_HPP
#define AV_RECORDING_PRINTER_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/replay/recorded_command.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;
class RecordingFilter;
class RecordingReader;
class TextBuffer;

struct RecordingPrinterProbe;

class RecordingPrinter {
public:
    static constexpr size_t   line_bytes = 512u;
    /* Beyond this a line would be mostly hex. */
    static constexpr uint32_t most_payload_bytes = 128u;
    /* `payload_bytes` of each command's payload are shown as hex; 0 shows
     * none, and more than most_payload_bytes shows most_payload_bytes. What
     * each context speaks is kept in `memory`. */
    RecordingPrinter(RecordingReader &reader, const RecordingFilter &filter, uint32_t payload_bytes,
                     BlockSource &memory);
    RecordingPrinter(const RecordingPrinter &) = delete;
    RecordingPrinter &operator=(const RecordingPrinter &) = delete;

    /* The next line, valid until the next call: ok, or not_found once the
     * recording is printed. */
    [[nodiscard]] Status next(const char *&line);
    uint64_t             lines() const;
    /* Whether the recording ended in damage, once printed. */
    bool                 damaged() const;

    bool invariants_hold() const;

private:
    enum class Stage : uint8_t { unopened, records, commands, done };

    /* The next record worth showing, or false at the end or at the damage. */
    bool take_record();
    /* Whether this record passes the filter; counts a batch's commands. */
    bool shown(const RecordHeader &record, const uint8_t *payload);
    /* The API half of the ids the context of `record` sends, or any_api. */
    uint16_t api_of(uint32_t context) const;
    /* Remembers what a setup record bound its context to; a context there is
     * no memory to remember is shown whatever its API. */
    void     learn(const RecordHeader &record, const uint8_t *payload);
    /* The next command line of the batch being shown, or false when it ends. */
    bool     next_command(TextBuffer &text);

    void append_prefix(TextBuffer &text) const;
    void append_record(TextBuffer &text) const;
    void append_setup(TextBuffer &text) const;
    void append_fault(TextBuffer &text) const;
    void append_blob(TextBuffer &text) const;
    void append_blob_bytes(TextBuffer &text) const;
    void append_command(TextBuffer &text, const RecordedCommand &command) const;
    void append_transport(TextBuffer &text, const RecordedCommand &command) const;
    void append_damage(TextBuffer &text) const;

    RecordingReader       &reader_;
    const RecordingFilter &filter_;
    uint32_t               payload_bytes_;
    Stage                  stage_;
    RecordHeader           record_;
    const uint8_t         *payload_;
    uint64_t               start_;
    uint32_t               commands_;
    uint32_t               command_offset_;
    uint64_t               lines_;
    bool                   damage_printed_;
    /* The API half of the command ids each context sends, by its id. */
    KeyedSlots<uint32_t, uint16_t> apis_;
    char                   line_[line_bytes];

    friend struct RecordingPrinterProbe;
};

}  // namespace av

#endif  // AV_RECORDING_PRINTER_HPP
