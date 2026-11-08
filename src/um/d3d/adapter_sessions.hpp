#ifndef AV_UM_ADAPTER_SESSIONS_HPP
#define AV_UM_ADAPTER_SESSIONS_HPP

#include "guest/kmt/avgpu_adapters.hpp"
#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/scope_exit.hpp"
#include "guest/transport/guest_connection.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/link_generations.hpp"
#include "proto/context/sub_api.hpp"
#include "core/trace/function_scope.hpp"
#include "um/common/srw_lock.hpp"
#include "um/d3d/d3d_session.hpp"
#include "um/d3d/slice_offer.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class Kmt;

template <typename Caps>
class AdapterSessions {
public:
    static constexpr uint32_t max_adapters = AvgpuAdapters::max_adapters;
    static constexpr uint32_t none = max_adapters;

    AdapterSessions(Kmt &kmt, BlockSource &memory, DriverLog &log, const char *refusal, uint64_t arena_bytes,
                    uint32_t select_command, uint32_t flush_command, const d3d::SliceOffer *offer);
    ~AdapterSessions();
    AdapterSessions(const AdapterSessions &) = delete;
    AdapterSessions &operator=(const AdapterSessions &) = delete;

    uint32_t         slot_of(uint32_t kmt_adapter, bool &learned);
    Caps            &caps(uint32_t slot);
    d3d::D3dSession *session(uint32_t slot);
    bool             exhausted() const;

private:
    static constexpr uint8_t  probe_ring_order = 16u;
    static constexpr uint32_t probe_reply_bytes = 64u * 1024u;
    static constexpr size_t   probe_staging_bytes = 16u * 1024u;

    [[nodiscard]] bool learn(Caps &caps, uint32_t kmt_adapter);

    Kmt                           &kmt_;
    BlockSource                   &memory_;
    DriverLog                     &lines_;
    const char                    *refusal_;
    uint64_t                       arena_bytes_;
    uint32_t                       select_command_;
    uint32_t                       flush_command_;
    const d3d::SliceOffer         *offer_;
    Caps                           caps_[max_adapters];
    ObjectStorage<icd::LinkGenerations<d3d::D3dSession>> sessions_[max_adapters];
    uint32_t                       adapters_[max_adapters];
    bool                           exhausted_;
    const char                    *learn_step_;
    Status                         learn_status_;
};

template <typename Caps>
AdapterSessions<Caps>::AdapterSessions(Kmt &kmt, BlockSource &memory, DriverLog &log, const char *refusal,
                                       uint64_t arena_bytes, uint32_t select_command,
                                       uint32_t flush_command, const d3d::SliceOffer *offer)
    : kmt_(kmt), memory_(memory), lines_(log), refusal_(refusal), arena_bytes_(arena_bytes),
      select_command_(select_command), flush_command_(flush_command), offer_(offer), caps_(),
      sessions_(), adapters_{}, exhausted_(false), learn_step_(""), learn_status_(Status::ok)
{
    AV_SCOPE("AdapterSessions::AdapterSessions");
    for (auto &slot : sessions_) static_cast<void>(slot.construct(memory));
}

template <typename Caps>
AdapterSessions<Caps>::~AdapterSessions()
{
    AV_SCOPE("AdapterSessions::~AdapterSessions");
    for (auto &slot : sessions_) slot.destroy();
}

template <typename Caps>
uint32_t AdapterSessions<Caps>::slot_of(uint32_t kmt_adapter, bool &learned)
{
    AV_SCOPE("AdapterSessions::slot_of");
    learned = false;
    exhausted_ = false;
    for (uint32_t slot = 0u; slot < max_adapters; slot++)
        if (adapters_[slot] == kmt_adapter && caps_[slot].learned()) return slot;
    for (uint32_t slot = 0u; slot < max_adapters; slot++) {
        if (caps_[slot].learned()) continue;
        if (!learn(caps_[slot], kmt_adapter)) {
            LogLine(lines_)
                .text(refusal_)
                .text(": ")
                .text(learn_step_)
                .text(", status ")
                .number(static_cast<uint32_t>(learn_status_))
                .write();
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
    learn_step_ = "the adapter would not open";
    learn_status_ = connection.open(kmt_adapter);
    if (learn_status_ != Status::ok) return false;
    ScopeExit        closed([&] { connection.close(); });
    SrwLock          unshared;
    icd::HostChannel probe(memory_, lines_,
                           icd::ChannelShape(SubApi::d3d, probe_ring_order, probe_reply_bytes, probe_staging_bytes,
                                             0u, 0u),
                           unshared);
    const Status opened = probe.open(connection);
    learn_step_ = "the probe context was refused";
    learn_status_ = opened;
    if (opened != Status::ok) {
        exhausted_ = opened == Status::no_memory;
        return false;
    }
    const bool learned = caps.learn(probe.transport(), kmt_, kmt_adapter);
    learn_step_ = "the host did not answer the capability queries";
    exhausted_ = !learned && probe.exhausted();
    probe.close(false);
    return learned;
}

template <typename Caps>
bool AdapterSessions<Caps>::exhausted() const
{
    AV_SCOPE("AdapterSessions::exhausted");
    return exhausted_;
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
    bool                   left_lost = false;
    d3d::D3dSession *const joined = sessions_[slot].object().join(left_lost, kmt_, memory_, lines_, arena_bytes_,
                                                                  select_command_, flush_command_, offer_);
    if (left_lost) lines_.write("the host faulted the adapter's session: new devices open one of their own");
    return joined;
}

}

#endif
