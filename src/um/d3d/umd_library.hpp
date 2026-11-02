#ifndef AV_UM_UMD_LIBRARY_HPP
#define AV_UM_UMD_LIBRARY_HPP

#include "icd/d3d/hresult.hpp"
#include "icd/d3d/umd_root.hpp"
#include "um/d3d/umd_drivers.hpp"
#include "um/common/driver_seams.hpp"
#include "um/common/lazy_driver.hpp"
#include "um/common/srw_lock.hpp"
#include "um/common/windows_calling_thread.hpp"
#include "um/trace/process_trace.hpp"

namespace av {

struct D3d10ddiargOpenAdapterRecordForward;

class UmdLibrary {
public:
    constexpr explicit UmdLibrary(ProcessTrace::Filter crashed);
    UmdLibrary(const UmdLibrary &) = delete;
    UmdLibrary &operator=(const UmdLibrary &) = delete;

    d3d::Hresult open_adapter10_2(void *args);
    d3d::Hresult open_adapter12(void *args);
    d3d::Hresult open_adapter9(void *args);
    long         crashed(void *pointers);
    void         ask_shared_rows();
    void         withdraw_shared_rows();

private:
    struct Seams {
        Seams();
        Seams(const Seams &) = delete;
        Seams &operator=(const Seams &) = delete;
        DriverSeams   process;
        SrwLock       lock;
        WindowsCallingThread caller;
        UmdDrivers    driver;
        d3d::UmdRoot  root;
    };


    LazyDriver<Seams> driver_;
};

constexpr UmdLibrary::UmdLibrary(ProcessTrace::Filter crashed) : driver_("avumd", crashed) {}

}

#endif
