#ifndef AV_UM_CL_ICD_LIBRARY_HPP
#define AV_UM_CL_ICD_LIBRARY_HPP

#include "icd/opencl/cl_icd_root.hpp"
#include "um/common/driver_seams.hpp"
#include "um/opencl/windows_d3d_adapter_source.hpp"
#include "um/opencl/windows_d3d11_bridge.hpp"
#include "um/opencl/windows_gl_share_source.hpp"
#include "um/opencl/windows_header_source.hpp"
#include "um/common/lazy_driver.hpp"
#include "um/common/srw_lock.hpp"
#include "um/common/windows_service_thread.hpp"
#include "um/trace/process_trace.hpp"

#include <stdint.h>

#include <CL/cl.h>

namespace av {

class ClIcdLibrary {
public:
    constexpr explicit ClIcdLibrary(ProcessTrace::Filter crashed);
    ClIcdLibrary(const ClIcdLibrary &) = delete;
    ClIcdLibrary &operator=(const ClIcdLibrary &) = delete;

    cl_int platform_ids(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms);
    void  *extension_function(const char *name);
    long   crashed(void *pointers);


private:
    struct Seams {
        Seams();
        Seams(const Seams &) = delete;
        Seams &operator=(const Seams &) = delete;
        DriverSeams      process;
        SrwLock          lock;
        SrwLock          send_lock;
        SrwLock          objects_lock;
        SrwLock          blocks_lock;
        WindowsServiceThread watcher;
        WindowsGlShareSource gl_share;
        WindowsHeaderSource  headers;
        WindowsD3dAdapterSource d3d_adapters;
        WindowsD3d11Bridge d3d11;
        clicd::ClIcdRoot root;
    };


    LazyDriver<Seams> driver_;
};

constexpr ClIcdLibrary::ClIcdLibrary(ProcessTrace::Filter crashed) : driver_("avcl", crashed) {}

}

#endif
