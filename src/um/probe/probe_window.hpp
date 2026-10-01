/* The window avglprobe.exe draws into (feature 050, T008 and T042): a class
 * registered once, a 64x64 overlapped window with its own DC, released and
 * destroyed at the end. Each step answers whether it worked and keeps
 * GetLastError, so the discovery run can report every step. Translation only. */
#ifndef AV_UM_PROBE_WINDOW_HPP
#define AV_UM_PROBE_WINDOW_HPP

#include <stdint.h>

namespace av {

class ProbeWindow {
public:
    static constexpr uint32_t side = 64u;

    ProbeWindow();
    ~ProbeWindow();
    ProbeWindow(const ProbeWindow &) = delete;
    ProbeWindow &operator=(const ProbeWindow &) = delete;

    bool     register_class();
    bool     create();
    bool     get_dc();
    /* All three in order; false at the first that failed. */
    bool     open();
    void    *window() const;
    void    *dc() const;
    /* The module the class was registered for: a surface is made for it. */
    void    *module() const;
    uint16_t atom() const;
    uint32_t last_error() const;

private:
    void    *instance_;
    void    *window_;
    void    *dc_;
    uint16_t atom_;
    uint32_t last_error_;
};

}  // namespace av

#endif  // AV_UM_PROBE_WINDOW_HPP
