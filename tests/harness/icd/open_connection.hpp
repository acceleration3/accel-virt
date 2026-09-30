/* An ICD connection opened on MockKmt's rvkgpu adapter, for the classes that
 * work over one: its lock records into a trace under 'S', and the host behind
 * its context is played through ScriptedHost. */
#ifndef AV_OPEN_CONNECTION_HPP
#define AV_OPEN_CONNECTION_HPP

#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/connection.hpp"

#include <cstdint>

namespace av {

class OpenConnection {
public:
    /* Adapters, device, queue, the transport's name and DMA allocations, its
     * mapping and its context. */
    static constexpr std::uint32_t link_objects = 7u;

    OpenConnection();
    OpenConnection(const OpenConnection &) = delete;
    OpenConnection &operator=(const OpenConnection &) = delete;

    MockKmt             &kmt();
    MockPlatform        &platform();
    ScriptedHost        &host();
    LockTrace           &trace();
    RecordingThreadLock &lock();
    icd::Connection     &connection();

private:
    MockKmt             kmt_;
    MockPlatform        platform_;
    ScriptedHost        host_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    icd::Connection     connection_;
};

}  // namespace av

#endif  // AV_OPEN_CONNECTION_HPP
