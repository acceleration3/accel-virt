#ifndef AV_CL_ICD_RIG_HPP
#define AV_CL_ICD_RIG_HPP

#include "core/log/keyed_log.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_d3d11_bridge.hpp"
#include "harness/icd/scripted_d3d_adapter_source.hpp"
#include "harness/icd/scripted_gl_share_source.hpp"
#include "harness/icd/scripted_header_source.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_icd_root.hpp"

namespace av {

class ClIcdRig {
public:
    static constexpr std::uint64_t adapter_luid = 0x0000000200001234ull;
    static constexpr std::uint64_t transport_clock_step = 1000u;

    ClIcdRig();
    ClIcdRig(const ClIcdRig &) = delete;
    ClIcdRig &operator=(const ClIcdRig &) = delete;

    MockPlatform        &platform();
    MockKmt             &kmt();
    ScriptedHost        &host();
    LockTrace           &trace();
    RecordingThreadLock &lock();
    RecordingThreadLock &send_lock();
    RecordingLog        &log();
    ScriptedThread      &watcher();
    ScriptedGlShareSource &gl_share();
    ScriptedHeaderSource  &headers();
    ScriptedD3dAdapterSource &d3d_adapters();
    ScriptedD3d11Bridge &d3d11();
    clicd::ClIcdRoot    &root();
    FakeClLoader        &loader();

private:
    MockPlatform         platform_;
    MockKmt              kmt_;
    ScriptedHost         host_;
    LockTrace            trace_;
    RecordingThreadLock  lock_;
    RecordingThreadLock  send_lock_;
    RecordingThreadLock  objects_lock_;
    RecordingThreadLock  blocks_lock_;
    RecordingLog         log_;
    KeyedLog             lines_;
    ScriptedThread       watcher_;
    ScriptedGlShareSource gl_share_;
    ScriptedHeaderSource  headers_;
    ScriptedD3dAdapterSource d3d_adapters_;
    ScriptedD3d11Bridge  d3d11_;
    clicd::ClIcdRoot     root_;
    FakeClLoader         loader_;
};

}

#endif
