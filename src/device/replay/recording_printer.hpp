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
    static constexpr uint32_t most_payload_bytes = 128u;
    RecordingPrinter(RecordingReader &reader, const RecordingFilter &filter, uint32_t payload_bytes,
                     BlockSource &memory);
    RecordingPrinter(const RecordingPrinter &) = delete;
    RecordingPrinter &operator=(const RecordingPrinter &) = delete;

    [[nodiscard]] Status next(const char *&line);
    uint64_t             lines() const;
    bool                 damaged() const;

    bool invariants_hold() const;

private:
    enum class Stage : uint8_t { unopened, records, commands, done };

    bool take_record();
    bool shown(const RecordHeader &record, const uint8_t *payload);
    uint16_t api_of(uint32_t context) const;
    void     learn(const RecordHeader &record, const uint8_t *payload);
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
    KeyedSlots<uint32_t, uint16_t> apis_;
    char                   line_[line_bytes];

    friend struct RecordingPrinterProbe;
};

}

#endif
