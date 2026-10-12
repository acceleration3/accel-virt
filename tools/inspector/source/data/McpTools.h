#ifndef DATA_MCP_TOOLS_H
#define DATA_MCP_TOOLS_H

// The inspector's MCP tools: what the window shows, as JSON, for a debugging agent. Each tool reads
// the Model under its lock, so the rates and stall verdicts are the window's own.
//
//   stalls        every stalled or lock-waiting thread of every process, with its stack - first stop
//   processes     the binaries found, alive or not, their threads and their last dump
//   threads       one process's threads: state, function, time in it, calls/s, locks, stack
//   state         one process's dump, filtered, optionally only what changed since the last round
//   connections   every device av context (fence, batches/s, ring bytes/s, worker, faults), sessions
//   surfaces      every GPU's shares (origin, kind, size, holders, writes/s) and exportable images
//   series        the history behind any number: list keys by substring, or one key's samples
//   sample_now    take a round now instead of waiting for the next

#include "mcp/Server.h"

namespace inspector {

class Model;

class McpTools {
public:
    explicit McpTools(Model& model);
    McpTools(const McpTools&) = delete;
    McpTools& operator=(const McpTools&) = delete;
    McpTools(McpTools&&) = delete;
    McpTools& operator=(McpTools&&) = delete;
    ~McpTools() = default;

    static mcp::ServerInfo Info();
    void Register(mcp::Server& server);

private:
    mcp::ToolResult Stalls(const JsonValue& arguments);
    mcp::ToolResult Processes(const JsonValue& arguments);
    mcp::ToolResult Threads(const JsonValue& arguments);
    mcp::ToolResult State(const JsonValue& arguments);
    mcp::ToolResult Connections(const JsonValue& arguments);
    mcp::ToolResult Surfaces(const JsonValue& arguments);
    mcp::ToolResult SeriesTool(const JsonValue& arguments);
    mcp::ToolResult SampleNow(const JsonValue& arguments);

    Model& m_model;
};

} // namespace inspector

#endif
