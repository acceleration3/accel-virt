/* A Direct3D driver's adapters: each kernel adapter a device was asked for,
 * the host's capabilities for it - learned once, the first time - and the one
 * session every device of this runtime on it shares (D3dSession). Direct3D
 * 9's, 11's and 12's drivers each keep one over their own Caps, which is made
 * empty and has learned() and learn(transport, kmt, adapter): the questions
 * it asks go over a channel opened for them alone and closed after. */
#ifndef AV_UM_ADAPTER_SESSIONS_HPP
#define AV_UM_ADAPTER_SESSIONS_HPP

#include "guest/kmt/avgpu_adapters.hpp"
#include "core/log/driver_log.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/scope_exit.hpp"
#include "guest/transport/guest_connection.hpp"
#include "icd/common/host_channel.hpp"
#include "proto/context/sub_api.hpp"
#include "core/trace/function_scope.hpp"
#include "um/common/srw_lock.hpp"
#include "um/d3d/d3d_session.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class Kmt;
class LogSink;

template <typename Caps>
class AdapterSessions {
public:
    static constexpr uint32_t max_adapters = AvgpuAdapters::max_adapters;
    /* The slot of no adapter: the host runs none of this runtime on it, or
     * every slot is taken. */
    static constexpr uint32_t none = max_adapters;

    /* `refusal` says the host answered no capabilities for an adapter. A
     * session opens an arena of `arena_bytes`, a slice store of
     * `slice_store_bytes` (none for 0) where the host draws in place, and
     * selects and flushes with the runtime's two commands (D3dCommandStream). */
    AdapterSessions(Kmt &kmt, BlockSource &memory, LogSink &log, const char *refusal, uint64_t arena_bytes,
                    uint64_t slice_store_bytes, uint32_t select_command, uint32_t flush_command);
    AdapterSessions(const AdapterSessions &) = delete;
    AdapterSessions &operator=(const AdapterSessions &) = delete;

    /* The slot of `kmt_adapter`, its capabilities learned on its first ask -
     * `learned` says whether that was this one - or none. */
    uint32_t         slot_of(uint32_t kmt_adapter, bool &learned);
    Caps            &caps(uint32_t slot);
    /* The session a new device on the slot joins, opened on its first ask;
     * null when there is no memory for one.
     * One the host faulted - its Direct3D device was lost - while devices of
     * it remain is left to them, and a new device gets a session of its own:
     * DWM makes its new device before it lets the removed one go, and that
     * device on the faulted session would have failed to be made, and the
     * desktop with it. */
    d3d::D3dSession *session(uint32_t slot);

private:
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
    uint32_t                       flush_command_;
    Caps                           caps_[max_adapters];
    /* Every adapter's sessions: the one new devices join, and the faulted
     * ones whose devices the runtime has not let go of yet, however many
     * faults that is. */
    ObjectPool<d3d::D3dSession>    sessions_;
    /* The session each slot's new devices join; null until one is asked for. */
    d3d::D3dSession               *current_[max_adapters];
    uint32_t                       adapters_[max_adapters];
};

template <typename Caps>
AdapterSessions<Caps>::AdapterSessions(Kmt &kmt, BlockSource &memory, LogSink &log, const char *refusal,
                                       uint64_t arena_bytes, uint64_t slice_store_bytes, uint32_t select_command,
                                       uint32_t flush_command)
    : kmt_(kmt), memory_(memory), log_(log), lines_(log, "avumd: "), refusal_(refusal), arena_bytes_(arena_bytes),
      slice_store_bytes_(slice_store_bytes),
      select_command_(select_command), flush_command_(flush_command), caps_(),
      sessions_(memory), current_{}, adapters_{}
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
    SrwLock          unshared;
    icd::HostChannel probe(memory_, log_, "avumd: ",
                           icd::ChannelShape(SubApi::d3d, probe_ring_order, probe_reply_bytes, probe_staging_bytes,
                                             0u, 0u),
                           unshared);
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
d3d::D3dSession *AdapterSessions<Caps>::session(uint32_t slot)
{
    AV_SCOPE("AdapterSessions::session");
    /* A faulted session whose last device went is closed; it goes now. */
    sessions_.destroy_if([this](const d3d::D3dSession &held) {
        for (const d3d::D3dSession *current : current_)
            if (current == &held) return false;
        return held.members() == 0u;
    });
    d3d::D3dSession *&current = current_[slot];
    if (current != nullptr && current->members() != 0u && current->lost()) {
        lines_.write("the host faulted the adapter's session: new devices open one of their own");
        current = nullptr;
    }
    if (current == nullptr &&
        sessions_.create(current, kmt_, memory_, log_, arena_bytes_, slice_store_bytes_, select_command_,
                         flush_command_) != Status::ok)
        current = nullptr;
    return current;
}

}  // namespace av

#endif  // AV_UM_ADAPTER_SESSIONS_HPP
