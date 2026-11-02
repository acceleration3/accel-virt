#ifndef AV_UM_STATE_SERVICE_HPP
#define AV_UM_STATE_SERVICE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ObjectSecurity;
class StateReporter;

class StateService {
public:
    static constexpr size_t section_bytes = 256u * 1024u;

    StateService();
    StateService(const StateService &) = delete;
    StateService &operator=(const StateService &) = delete;

    [[nodiscard]] Status start(const char *module, uint32_t pid, StateReporter &reporter, ObjectSecurity &security);
    void                 serve();

private:
    [[nodiscard]] bool make(const char *scope, const char *module, uint32_t pid, ObjectSecurity &security);
    void               close();

    StateReporter *reporter_;
    void          *dump_;
    void          *done_;
    void          *section_;
    uint8_t       *view_;
    void          *thread_;
};

}

#endif
