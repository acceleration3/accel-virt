/* avrecord: prints a wire recording, or replays it into an executor library
 * (contracts/avrecord.md). The shim, so it holds the platform and nothing
 * else - argv, a mapping, a dlopen, and writing to standard output. Every
 * decision is in src/device/recording/, where the coverage gate measures it:
 * what the arguments mean is RecordingToolRequest's, what a line says is
 * RecordingPrinter's, and what differs is RecordingReplay's.
 */
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "device/execution/executor_library.hpp"
#include "device/replay/recording_filter.hpp"
#include "device/replay/recording_printer.hpp"
#include "device/replay/recording_reader.hpp"
#include "device/replay/recording_replay.hpp"
#include "device/replay/recording_tool_request.hpp"
#include "device/shim/mapped_file.hpp"
#include "device/shim/posix_executor_module.hpp"

#include <cstdint>
#include <cstdio>

namespace {

int print(av::RecordingReader &reader, const av::RecordingFilter &filter, std::uint32_t payload_bytes)
{
    AV_SCOPE("print");
    av::RecordingPrinter printer(reader, filter, payload_bytes);
    const char          *line = nullptr;
    while (printer.next(line) == av::Status::ok) std::printf("%s\n", line);
    return printer.damaged() ? 1 : 0;
}

int replay(av::RecordingReader &reader, const av::RecordingFilter &filter,
           const av::RecordingToolRequest &request)
{
    AV_SCOPE("replay");
    av::PosixExecutorModule module;
    if (module.open(request.executor()) != av::Status::ok) {
        std::fprintf(stderr, "avrecord: %s is no executor library\n", request.executor());
        return 2;
    }
    av::ExecutorLibrary library;
    if (library.accept(module.ops()) != av::Status::ok) {
        std::fprintf(stderr, "avrecord: %s answers a table this ABI does not accept\n",
                     request.executor());
        return 2;
    }
    av::RecordingReplay replay_run(library);
    replay_run.use_gpu(request.gpu());
    const av::Status ran = replay_run.run(reader, filter);
    for (std::uint32_t index = 0u; index < replay_run.findings(); index++)
        std::printf("%s\n", replay_run.finding(index));
    std::printf("replay: %u contexts, %llu commands, %u differences\n", replay_run.contexts(),
                static_cast<unsigned long long>(replay_run.commands()), replay_run.findings());
    if (ran != av::Status::ok) std::printf("damaged at %08llX: %s\n",
                                           static_cast<unsigned long long>(reader.damage_offset()),
                                           reader.damage());
    return (ran == av::Status::ok && replay_run.findings() == 0u) ? 0 : 1;
}

}  // namespace

int main(int count, char **arguments)
{
    av::RecordingFilter      filter;
    av::RecordingToolRequest request;
    if (request.parse(count, arguments, filter) != av::Status::ok) {
        std::fprintf(stderr, "avrecord: %s\n", request.reason());
        return 2;
    }
    av::MappedFile file;
    if (file.open(request.recording()) != av::Status::ok) {
        std::fprintf(stderr, "avrecord: %s could not be read\n", request.recording());
        return 2;
    }
    av::RecordingReader reader(file.bytes(), file.length());
    return request.action() == av::RecordingToolRequest::Action::print
               ? print(reader, filter, request.payload_bytes())
               : replay(reader, filter, request);
}
