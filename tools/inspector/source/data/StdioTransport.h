#ifndef DATA_STDIO_TRANSPORT_H
#define DATA_STDIO_TRANSPORT_H

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

}

#endif
