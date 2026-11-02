#ifndef AV_INSPECT_PROGRAM_HPP
#define AV_INSPECT_PROGRAM_HPP

#include "core/support/status.hpp"
#include "guest/inspect/command_line.hpp"
#include "proto/escape/trace_escape_record.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;
class InspectHost;
class Kmt;
class SnapshotFile;
class TextBuffer;

class InspectProgram {
public:
    enum class Exit : uint32_t { written = 0u, usage = 1u, unwritten = 2u };

    static constexpr uint32_t state_timeout_ms = 5000u;
    static constexpr size_t   state_bytes = 256u * 1024u;
    static constexpr size_t   list_bytes = 256u * 1024u;
    static constexpr uint32_t max_processes = 4096u;
    static constexpr size_t   max_module = 15u;

    InspectProgram(InspectHost &host, Kmt &kmt, BlockSource &memory);
    InspectProgram(const InspectProgram &) = delete;
    InspectProgram &operator=(const InspectProgram &) = delete;

    Exit run(const uint16_t *line);

    const char *usage() const;

private:
    Exit list(const char *out);
    Exit trace(uint32_t pid, const char *module, const char *out);
    Exit state(uint32_t pid, const char *module, const char *out);
    Exit kernel(TraceEscapeRecord::Kind kind, const char *out);
    Exit dump_file(SnapshotFile &file, const char *section);
    void name(TextBuffer &out, const char *prefix, const char *kind, const char *module, uint32_t pid) const;
    bool module_name(const char *module) const;
    Exit table_file(const char *out, uint64_t clock, const uint8_t *bytes, size_t length, const char *source);
    Exit finish(Status written) const;

    InspectHost &host_;
    Kmt         &kmt_;
    BlockSource &memory_;
    CommandLine  line_;
};

}

#endif
