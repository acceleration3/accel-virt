#ifndef AV_ICD_PRESENT_PACING_HPP
#define AV_ICD_PRESENT_PACING_HPP

#include "core/support/object_storage.hpp"
#include "icd/common/frame_pacer.hpp"

#include <stdint.h>

namespace av {
class DriverLog;
class Kmt;
}

namespace av::icd {

class PresentPacing {
public:
    static constexpr uint32_t max_blanks = 4u;

    PresentPacing();
    ~PresentPacing();
    PresentPacing(const PresentPacing &) = delete;
    PresentPacing &operator=(const PresentPacing &) = delete;

    [[nodiscard]] Status bind(Kmt &kmt, DriverLog &log, uint32_t adapter, uint32_t device);
    void                 unbind();

    [[nodiscard]] bool ask(int32_t blanks);
    uint32_t           blanks() const;
    bool               admits();
    uint32_t           next_admit_milliseconds() const;
    void               after_present();
    uint64_t           waited() const;

    bool invariants_hold() const;

private:
    ObjectStorage<FramePacer> pacer_;
    uint32_t                  blanks_;
    uint64_t                  waited_;

    friend struct PresentPacingProbe;
};

}

#endif
