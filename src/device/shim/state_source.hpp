#ifndef AV_STATE_SOURCE_HPP
#define AV_STATE_SOURCE_HPP

#include <stddef.h>

namespace av {

class StateSource {
public:
    StateSource() = default;
    StateSource(const StateSource &) = delete;
    StateSource &operator=(const StateSource &) = delete;

    virtual size_t dump(char *buffer, size_t capacity, const char *&error) = 0;

protected:
    ~StateSource() = default;
};

}

#endif
