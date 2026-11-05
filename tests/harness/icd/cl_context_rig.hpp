#ifndef AV_CL_CONTEXT_RIG_HPP
#define AV_CL_CONTEXT_RIG_HPP

#include "core/log/keyed_log.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/icd_connection.hpp"
#include "icd/common/shared_handles.hpp"

namespace av {

class ClContextRig {
public:
    static constexpr std::uint32_t link_objects = 3u;

    ClContextRig();
    ClContextRig(const ClContextRig &) = delete;
    ClContextRig &operator=(const ClContextRig &) = delete;

    MockPlatform           &platform();
    MockKmt                &kmt();
    ScriptedHost           &host();
    LockTrace              &trace();
    RecordingThreadLock    &send_lock();
    RecordingThreadLock    &blocks_lock();
    RecordingLog           &log();
    KeyedLog               &lines();
    av::icd::IcdConnection &connection();
    av::icd::SharedHandles &handles();

private:
    MockPlatform           platform_;
    MockKmt                kmt_;
    ScriptedHost           host_;
    LockTrace              trace_;
    RecordingThreadLock    send_lock_;
    RecordingThreadLock    blocks_lock_;
    RecordingLog           log_;
    KeyedLog               lines_;
    av::icd::IcdConnection connection_;
    RecordingThreadLock    handles_lock_;
    av::icd::SharedHandles handles_;
};

}

#endif
