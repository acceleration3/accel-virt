/* A Direct3D driver's adapters: each kernel adapter a device was asked for,
 * the host's capabilities for it - learned once, the first time - and the one
 * session every device of this runtime on it shares (D3dSession). Direct3D
 * 9's, 11's and 12's drivers each keep one over their own Caps, which is made
 * empty and has learned() and learn(transport, kmt, adapter): the questions
 * it asks go over a channel opened for them alone and closed after. */
#ifndef AV_UM_ADAPTER_SESSIONS_HPP
#define AV_UM_ADAPTER_SESSIONS_HPP

#include "core/log/driver_log.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/scope_exit.hpp"
#include "guest/transport/guest_connection.hpp"
#include "icd/common/host_channel.hpp"
#include "proto/context/sub_api.hpp"
#include "core/trace/function_scope.hpp"
#include "um/d3d/d3d_session.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class Kmt;
class LogSink;

template <typename Caps>
class AdapterSessions {
public:
    static constexpr uint32_t max_adapters = 8u;
    /* The slot of no adapter: the host runs none of this runtime on it, or
     * every slot is taken. */
    static constexpr uint32_t none = max_adapters;

    /* `refusal` says the host answered no capabilities for an adapter. A
     * session opens an arena of `arena_bytes`, a slice store of
     * `slice_store_bytes` (none for 0) where the host draws in place, and
     * selects, finishes and flushes with the runtime's three commands
     * (D3dCommandStream). */
    AdapterSessions(Kmt &kmt, BlockSource &memory, LogSink &log, const char *refusal, uint64_t arena_bytes,
                    uint64_t slice_store_bytes, uint32_t select_command, uint32_t finish_command,
                    uint32_t flush_command);
    AdapterSessions(const AdapterSessions &) = delete;
    AdapterSessions &operator=(const AdapterSessions &) = delete;

    /* The slot of `kmt_adapter`, its capabilities learned on its first ask -
     * `learned` says whether that was this one - or none. */
    uint32_t         slot_of(uint32_t kmt_adapter, bool &learned);
    Caps            &caps(uint32_t slot);
    /* The session a new device on the slot joins, opened on its first ask.
     * One the host faulted - its Direct3D device was lost - while devices of
     * it remain is left to them, and a new device gets a session of its own:
     * DWM makes its new device before it lets the removed one go, and that
     * device on the faulted session would have failed to be made, and the
     * desktop with it. */
    d3d::D3dSession &session(uint32_t slot);

private:
    /* Sessions one adapter may have at once: the one new devices join, and
     * those faulted ones whose devices the runtime has not let go of yet. A
     * slot whose every session still has devices gives the current one, as
     * before a fault. */
    static constexpr uint32_t generations = 4u;

    /* The channel the capabilities are asked over: one question at a time,
     * small enough answers to need no arena. */
    static constexpr uint8_t  probe_ring_order = 16u;
    static constexpr uint32_t probe_reply_bytes = 64u * 1024u;
    static constexpr size_t   probe_staging_bytes = 16u * 1024u;

    /* `caps` learned over a channel opened on `kmt_adapter`. */
    [[nodiscard]] bool learn(Caps &caps, uint32_t kmt_adapter);

    Kmt                           &kmt_;
    BlockSource                   &memory_;
    LogSink                       &log_;
    DriverLog                      lines_;
    const char                    *refusal_;
    uint64_t                       arena_bytes_;
    uint64_t slice_store_bytes_;
    uint32_t                       select_command_;
    uint32_t                       finish_command_;
    uint32_t                       flush_command_;
    Caps                           caps_[max_adapters];
    ObjectStorage<d3d::D3dSession> sessions_[max_adapters][generations];
    /* Which of a slot's sessions new devices join. */
    uint32_t                       current_[max_adapters];
    uint32_t                       adapters_[max_adapters];
};

template <typename Caps>
AdapterSessions<Caps>::AdapterSessions(Kmt &kmt, BlockSource &memory, LogSink &log, const char *refusal,
                                       uint64_t arena_bytes, uint64_t slice_store_bytes, uint32_t select_command,
                                       uint32_t finish_command, uint32_t flush_command)
    : kmt_(kmt), memory_(memory), log_(log), lines_(log, "avumd: "), refusal_(refusal), arena_bytes_(arena_bytes),
      slice_store_bytes_(slice_store_bytes),
      select_command_(select_command), finish_command_(finish_command), flush_command_(flush_command), caps_(),
      sessions_(), current_{}, adapters_{}
{
    AV_SCOPE("AdapterSessions::AdapterSessions");
}

template <typename Caps>
uint32_t AdapterSessions<Caps>::slot_of(uint32_t kmt_adapter, bool &learned)
{
    AV_SCOPE("AdapterSessions::slot_of");
    learned = false;
    for (uint32_t slot = 0u; slot < max_adapters; slot++)
        if (adapters_[slot] == kmt_adapter && caps_[slot].learned()) return slot;
    for (uint32_t slot = 0u; slot < max_adapters; slot++) {
        if (caps_[slot].learned()) continue;
        if (!learn(caps_[slot], kmt_adapter)) {
            lines_.write(refusal_);
            return none;
        }
        adapters_[slot] = kmt_adapter;
        learned = true;
        return slot;
    }
    return none;
}

template <typename Caps>
bool AdapterSessions<Caps>::learn(Caps &caps, uint32_t kmt_adapter)
{
    AV_SCOPE("AdapterSessions::learn");
    GuestConnection connection(kmt_);
    if (connection.open(kmt_adapter) != Status::ok) return false;
    ScopeExit        closed([&] { connection.close(); });
    icd::HostChannel probe(memory_, log_, "avumd: ",
                           icd::ChannelShape(SubApi::d3d, probe_ring_order, probe_reply_bytes, probe_staging_bytes,
                                             0u, 0u));
    if (probe.open(connection) != Status::ok) return false;
    const bool learned = caps.learn(probe.transport(), kmt_, kmt_adapter);
    probe.close(false);
    return learned;
}

template <typename Caps>
Caps &AdapterSessions<Caps>::caps(uint32_t slot)
{
    AV_SCOPE("AdapterSessions::caps");
    return caps_[slot];
}

template <typename Caps>
d3d::D3dSession &AdapterSessions<Caps>::session(uint32_t slot)
{
    AV_SCOPE("AdapterSessions::session");
    ObjectStorage<d3d::D3dSession> *const rows = sessions_[slot];
    uint32_t                             &current = current_[slot];
    /* A faulted session whose last device went is closed; it goes now. */
    for (uint32_t row = 0u; row < generations; row++)
        if (row != current && rows[row].constructed() && rows[row].object().members() == 0u) rows[row].destroy();
    if (rows[current].constructed() && rows[current].object().members() != 0u && rows[current].object().lost()) {
        for (uint32_t row = 0u; row < generations; row++) {
            if (rows[row].constructed()) continue;
            lines_.write("the host faulted the adapter's session: new devices open one of their own");
            current = row;
            break;
        }
    }
    if (!rows[current].constructed())
        static_cast<void>(rows[current].construct(kmt_, memory_, log_, arena_bytes_, slice_store_bytes_,
                                                  select_command_, finish_command_, flush_command_));
    return rows[current].object();
}

}  // namespace av

#endif  // AV_UM_ADAPTER_SESSIONS_HPP
