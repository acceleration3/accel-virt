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
    bool     open();
    void    *window() const;
    void    *dc() const;
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

}

#endif
