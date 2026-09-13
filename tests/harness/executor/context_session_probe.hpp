/* Reaches into a ContextSession so invariant tests can corrupt it, or the
 * imports it holds. Declared a friend by ContextSession; nothing outside the
 * tests uses it. */
#ifndef AV_CONTEXT_SESSION_PROBE_HPP
#define AV_CONTEXT_SESSION_PROBE_HPP

#include "device/executor/context_session.hpp"

namespace av {

class WireArena;

class ImportTable;

struct ContextSessionProbe {
    explicit ContextSessionProbe(ContextSession &target);

    ImportTable &imports();
    WireArena   &arena();
    void         drop_backend();
    void         set_faulted(bool faulted);

    ContextSession &session;
};

}  // namespace av

#endif  // AV_CONTEXT_SESSION_PROBE_HPP
