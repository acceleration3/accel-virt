#ifndef AV_OPEN_CONNECTION_HPP
#define AV_OPEN_CONNECTION_HPP

#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/connection.hpp"

#include <cstdint>

namespace av {

class OpenConnection {
public:
    static constexpr std::uint32_t link_objects = 7u;

    OpenConnection();
    OpenConnection(const OpenConnection &) = delete;
    OpenConnection &operator=(const OpenConnection &) = delete;

    MockKmt             &kmt();
    MockPlatform        &platform();
    ScriptedHost        &host();
    LockTrace           &trace();
    RecordingThreadLock &lock();
    RecordingLog        &log();
    icd::Connection     &connection();

private:
    MockKmt             kmt_;
    MockPlatform        platform_;
    ScriptedHost        host_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    RecordingLog        log_;
    icd::Connection     connection_;
};

}

#endif
