#ifndef AV_SLICE_CHANNEL_RIG_HPP
#define AV_SLICE_CHANNEL_RIG_HPP

#include "core/log/driver_log.hpp"
#include "guest/transport/guest_connection.hpp"
#include "harness/executor/scripted_clock.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/host_channel.hpp"

#include <cstdint>

namespace av {

class SliceChannelRig {
public:
    SliceChannelRig();
    ~SliceChannelRig();
    SliceChannelRig(const SliceChannelRig &) = delete;
    SliceChannelRig &operator=(const SliceChannelRig &) = delete;

    void will_open();
    void will_refuse_open();
    void will_close();

    MockPlatform        &platform();
    MockKmt             &kmt();
    ScriptedHost        &host();
    ScriptedClock       &clock();
    RecordingLog        &sink();
    DriverLog           &lines();
    RecordingThreadLock &lock();
    RecordingThreadLock &stream_lock();
    icd::HostChannel    &channel();
    std::uint32_t        opened() const;

private:
    MockPlatform        platform_;
    MockKmt             kmt_;
    GuestConnection     connection_;
    ScriptedHost        host_;
    RecordingLog        sink_;
    DriverLog           lines_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    RecordingThreadLock stream_lock_;
    ScriptedClock       clock_;
    icd::HostChannel    channel_;
    std::uint64_t       next_memory_;
    std::uint32_t       opened_;
};

}

#endif
