#include "harness/executor/context_session_probe.hpp"

namespace av {

ContextSessionProbe::ContextSessionProbe(ContextSession &target) : session(target) {}

WireArena &ContextSessionProbe::arena() { return session.arena_; }

ImportTable &ContextSessionProbe::imports() { return session.imports_; }

void ContextSessionProbe::drop_backend() { session.backend_ = nullptr; }

void ContextSessionProbe::drop_source() { session.source_ = nullptr; }

void ContextSessionProbe::set_faulted(bool faulted) { session.faulted_ = faulted; }

}  // namespace av
