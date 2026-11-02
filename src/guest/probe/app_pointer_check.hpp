#ifndef AV_APP_POINTER_CHECK_HPP
#define AV_APP_POINTER_CHECK_HPP

#include <stdint.h>

namespace av {

class ScenarioSteps;
struct AppPointerCheckProbe;

class AppPointerCheck {
public:
    static constexpr uint32_t bytes = 64u * 1024u;

    AppPointerCheck();
    AppPointerCheck(const AppPointerCheck &) = delete;
    AppPointerCheck &operator=(const AppPointerCheck &) = delete;

    const uint8_t *pattern() const;
    uint8_t       *copied();
    void           judge(ScenarioSteps &steps, const char *const names[3], int32_t result, uint32_t aliased);
    uint32_t       matching() const;

    bool invariants_hold() const;

private:
    uint8_t pattern_[bytes];
    uint8_t copied_[bytes];

    friend struct AppPointerCheckProbe;
};

}

#endif
