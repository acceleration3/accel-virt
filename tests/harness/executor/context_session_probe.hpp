#ifndef AV_CONTEXT_SESSION_PROBE_HPP
#define AV_CONTEXT_SESSION_PROBE_HPP

#include "executor/context_session.hpp"

namespace av {

class WireArena;

class ImportTable;

struct ContextSessionProbe {
    explicit ContextSessionProbe(ContextSession &target);

    ImportTable &imports();
    WireArena   &arena();
    void         drop_backend();
    void         drop_source();
    void         set_faulted(bool faulted);

    ContextSession &session;
};

}

#endif
