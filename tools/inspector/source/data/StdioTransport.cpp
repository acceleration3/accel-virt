#include "data/StdioTransport.h"

#include <errno.h>
#include <poll.h>
#include <unistd.h>

namespace inspector {

mcp::ReceiveStatus StdioTransport::Receive(String& message, Milliseconds<i64> timeout) {
    for (;;) {
        if (Optional<String> line = m_framer.Next()) {
            message = RValueCast(*line);
            return mcp::ReceiveStatus::Message;
        }
        if (m_closed || m_framer.IsOverflowed()) {
            return mcp::ReceiveStatus::Closed;
        }
        pollfd ready{STDIN_FILENO, POLLIN, 0};
        const int waited = poll(&ready, 1, static_cast<int>(timeout.Count()));
        if (waited == 0) {
            return mcp::ReceiveStatus::Timeout;
        }
        if (waited < 0) {
            if (errno == EINTR) {
                return mcp::ReceiveStatus::Timeout;
            }
            return mcp::ReceiveStatus::Closed;
        }
        char chunk[65536];
        const ssize_t got = read(STDIN_FILENO, chunk, sizeof chunk);
        if (got <= 0) {
            m_closed = true;
            continue; // a last line may still be buffered
        }
        m_framer.Append(StringView(chunk, static_cast<usize>(got)));
    }
}

bool StdioTransport::Send(StringView message) {
    const char* data = message.GetBuffer();
    usize left = message.GetSize();
    while (left > 0) {
        const ssize_t wrote = write(STDOUT_FILENO, data, left);
        if (wrote <= 0) {
            if (wrote < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        data += wrote;
        left -= static_cast<usize>(wrote);
    }
    return write(STDOUT_FILENO, "\n", 1) == 1;
}

} // namespace inspector
