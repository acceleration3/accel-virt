/* The three D3D probes (feature 047, T028): each makes its API's work over the
 * DLL beside it - DXVK's d3d9.dll and d3d11.dll, vkd3d-proton's d3d12.dll -
 * runs every step, writes the report to standard output for the guest agent to
 * carry back, and answers the failures as the exit code. Translation only:
 * what each probe decides is in src/guest/suite.
 *
 * Three classes rather than one with a switch: each holds its own API's seam
 * and the window that API needs, and D3D12 needs none. The D3D9 probe runs
 * WPF's presentation on its window after the clear (WpfPresentWork), reading
 * the window back through GDI. */
#ifndef AV_D3D_PROBE_PROGRAM_HPP
#define AV_D3D_PROBE_PROGRAM_HPP

#include "guest/suite/guest_report.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/probe/gdi_window_frame.hpp"
#include "um/probe/d3d/windows_d3d11_entry.hpp"
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

    /* The process exit code: how many steps failed. */
    uint32_t run();

private:
    GdiKmt             kmt_;
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
    GuestReport       report_;
    WindowsImageSink  images_;
    ProbeWindow       window_;
    WindowsD3d11Entry d3d11_;
};

class D3d12ProbeProgram {
public:
    D3d12ProbeProgram();
    D3d12ProbeProgram(const D3d12ProbeProgram &) = delete;
    D3d12ProbeProgram &operator=(const D3d12ProbeProgram &) = delete;

    uint32_t run();

private:
    GdiKmt            kmt_;
    GuestReport       report_;
    WindowsImageSink  images_;
    WindowsD3d12Entry d3d12_;
};

}  // namespace av

#endif  // AV_D3D_PROBE_PROGRAM_HPP
