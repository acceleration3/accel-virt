/* rvkinspect.exe (docs/trace.md): reads a process's trace table or state
 * dump, or the driver's, and writes it to the file --out names. Every answer
 * is a file; the exit code says only whether the command was understood and
 * the file written. */
#ifndef AV_INSPECT_PROGRAM_HPP
#define AV_INSPECT_PROGRAM_HPP

#include "core/support/status.hpp"
#include "guest/inspect/command_line.hpp"
#include "proto/trace/trace_escape_record.hpp"

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

    /* The command in `line`, as GetCommandLineW gives it. */
    Exit run(const uint16_t *line);

    /* What run() tells a caller who asked for something it does not do. */
    const char *usage() const;

private:
    Exit list(const char *out);
    Exit trace(uint32_t pid, const char *module, const char *out);
    Exit state(uint32_t pid, const char *module, const char *out);
    Exit kernel(TraceEscapeRecord::Kind kind, const char *out);
    Exit dump_file(SnapshotFile &file, const char *section);
    /* `<prefix>rvk-<kind>-<module>-<pid>`. */
    void name(TextBuffer &out, const char *prefix, const char *kind, const char *module, uint32_t pid) const;
    bool module_name(const char *module) const;
    /* The table in `bytes` as a kind 1 file, or the reason it is none as kind 3. */
    Exit table_file(const char *out, uint64_t clock, const uint8_t *bytes, size_t length, const char *source);
    Exit finish(Status written) const;

    InspectHost &host_;
    Kmt         &kmt_;
    BlockSource &memory_;
    CommandLine  line_;
};

}  // namespace av

#endif  // AV_INSPECT_PROGRAM_HPP
