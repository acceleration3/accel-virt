#ifndef DATA_STDIO_TRANSPORT_H
#define DATA_STDIO_TRANSPORT_H

// MCP's stdio transport: one JSON-RPC message per line on stdin, each reply one line on stdout.
// What an MCP client that launches the server as a subprocess speaks. Nothing else may write to
// stdout while it serves.

#include "mcp/Transport.h"

namespace inspector {

class StdioTransport final : public mcp::Transport {
public:
    StdioTransport() = default;
    ~StdioTransport() override = default;
    StdioTransport(const StdioTransport&) = delete;
    StdioTransport& operator=(const StdioTransport&) = delete;
    StdioTransport(StdioTransport&&) = delete;
    StdioTransport& operator=(StdioTransport&&) = delete;

    mcp::ReceiveStatus Receive(String& message, Milliseconds<i64> timeout) override;
    bool Send(StringView message) override;

private:
    mcp::LineFramer m_framer;
    bool m_closed = false;
};

} // namespace inspector

#endif
