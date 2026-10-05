/* An export connection's table, written past what any request can make it,
 * so a test breaks each invariant on its own. */
#ifndef AV_EXPORT_CONNECTION_PROBE_HPP
#define AV_EXPORT_CONNECTION_PROBE_HPP

#include "device/export/export_connection.hpp"

#include <cstdint>

namespace av {

/* The connection's table, written past what any request can make it. */
struct ExportConnectionProbe {
    static void hold(ExportConnection &connection, std::uint32_t held, std::uint64_t surface)
    {
        connection.held_ = held;
        for (std::uint32_t index = 0u; index < held && index < ExportConnection::max_surfaces; index++)
            connection.surfaces_[index] = surface + index;
    }
    static void name(ExportConnection &connection, std::uint32_t index, std::uint64_t surface)
    {
        connection.surfaces_[index] = surface;
    }
    static void describe(ExportConnection &connection, int descriptor) { connection.descriptor_ = descriptor; }
};

}  // namespace av

#endif  // AV_EXPORT_CONNECTION_PROBE_HPP
