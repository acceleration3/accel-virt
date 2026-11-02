#ifndef AV_D3D_PROBE_PROGRAM_HPP
#define AV_D3D_PROBE_PROGRAM_HPP

#include "guest/probe/host_loss_request.hpp"
#include "guest/suite/guest_report.hpp"
#include "um/common/process_heap.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/probe/gdi_window_frame.hpp"
#include "um/probe/d3d/windows_d3d11_deferred_entry.hpp"
#include "um/probe/d3d/windows_d3d11_entry.hpp"
#include "um/probe/d3d/windows_d3d11_shared_entry.hpp"
#include "um/probe/d3d/windows_d3d11_tir_entry.hpp"
#include "um/probe/d3d/windows_d3d12_entry.hpp"
#include "um/probe/d3d/windows_d3d9_entry.hpp"
#include "um/probe/d3d/windows_d3d9ex_entry.hpp"
#include "um/probe/probe_window.hpp"
#include "um/probe/windows_image_sink.hpp"

#include <stdint.h>

namespace av {

class D3d9ProbeProgram {
public:
    D3d9ProbeProgram();
    D3d9ProbeProgram(const D3d9ProbeProgram &) = delete;
    D3d9ProbeProgram &operator=(const D3d9ProbeProgram &) = delete;

    uint32_t run();

private:
    GdiKmt             kmt_;
    ProcessHeap        heap_;
    GuestReport        report_;
    WindowsImageSink   images_;
    ProbeWindow        window_;
    GdiWindowFrame     windows_;
    WindowsD3d9Entry   d3d9_;
    WindowsD3d9ExEntry d3d9ex_;
};

class D3d11ProbeProgram {
public:
    D3d11ProbeProgram();
    D3d11ProbeProgram(const D3d11ProbeProgram &) = delete;
    D3d11ProbeProgram &operator=(const D3d11ProbeProgram &) = delete;

    uint32_t run();

private:
    GdiKmt            kmt_;
    ProcessHeap       heap_;
    GuestReport       report_;
    WindowsImageSink  images_;
    ProbeWindow               window_;
    WindowsD3d11Entry         d3d11_;
    WindowsD3d11DeferredEntry deferred_;
    WindowsD3d11SharedEntry   shared_;
    WindowsD3d11TirEntry      tir_;
    WindowsD3d11SharedEntry   lost_;
    WindowsD3d11SharedEntry   remade_;
    HostLossRequest           loss_;
};

class D3d12ProbeProgram {
public:
    D3d12ProbeProgram();
    D3d12ProbeProgram(const D3d12ProbeProgram &) = delete;
    D3d12ProbeProgram &operator=(const D3d12ProbeProgram &) = delete;

    uint32_t run();

private:
    GdiKmt            kmt_;
    ProcessHeap       heap_;
    GuestReport       report_;
    WindowsImageSink  images_;
    WindowsD3d12Entry d3d12_;
};

}

#endif
