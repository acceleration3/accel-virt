#ifndef DATA_MCP_TOOLS_H
#define DATA_MCP_TOOLS_H

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

}

#endif
