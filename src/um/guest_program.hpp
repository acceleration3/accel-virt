/* rvktest.exe: the in-guest suite over gdi32, its report written to standard
 * output for the guest agent to carry back, its failures the exit code. */
#ifndef AV_GUEST_PROGRAM_HPP
#define AV_GUEST_PROGRAM_HPP

#include "guest/suite/guest_report.hpp"
#include "guest/suite/guest_suite.hpp"
#include "um/gdi_kmt.hpp"

#include <stdint.h>

namespace av {

class GuestProgram {
public:
    /* What the command line asked the program to run. */
    enum class Run : uint8_t { suite, timeout, rvk };

    /* The whole suite, with the long fence run only when the command line asks
     * for it: `rvktest.exe <seconds>` (feature 039, US3). With `timeout_only`
     * the command line asked for `tdr` instead, and only the timeout scenario
     * runs: the host has been told to hold a context, and every other scenario
     * would be measuring that (feature 040, US3). With `rvk` only the scenarios
     * that send work to the host's executor library run (feature 044). */
    GuestProgram(uint32_t stress_seconds, Run run);
    GuestProgram(const GuestProgram &) = delete;
    GuestProgram &operator=(const GuestProgram &) = delete;

    /* The process exit code: how many steps failed. */
    uint32_t run();

private:
    void write_report() const;

    GdiKmt      kmt_;
    GuestReport report_;
    GuestSuite  suite_;
    Run         run_;
};

}  // namespace av

#endif  // AV_GUEST_PROGRAM_HPP
