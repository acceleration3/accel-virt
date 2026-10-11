/* An export connection's table, written past what any request can make it,
 * so a test breaks each invariant on its own. */
#ifndef AV_EXPORT_CONNECTION_PROBE_HPP
#define AV_EXPORT_CONNECTION_PROBE_HPP

#include "device/export/export_connection.hpp"

#include <cstdint>

namespace av {

/* The connection's table, written past what any request can make it. */
struct ExportConnectionProbe {
    /* `held` surfaces from `surface` up, the list grown to take them. */
    static void hold(ExportConnection &connection, std::uint32_t held, std::uint64_t surface)
    {
        while (connection.room_ < held) {
            connection.held_ = connection.room_;
            if (!connection.room_for_one()) return;
        }
        connection.held_ = held;
        for (std::uint32_t index = 0u; index < held; index++) connection.surfaces_[index] = surface + index;
    }
    /* More held than the list has room for, which nothing writes. */
    static void overfill(ExportConnection &connection) { connection.held_ = connection.room_ + 1u; }
    static void name(ExportConnection &connection, std::uint32_t index, std::uint64_t surface)
    {
        connection.surfaces_[index] = surface;
    }
    static void describe(ExportConnection &connection, int descriptor) { connection.descriptor_ = descriptor; }
};

}  // namespace av

#endif  // AV_EXPORT_CONNECTION_PROBE_HPP
