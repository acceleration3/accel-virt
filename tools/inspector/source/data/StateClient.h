#ifndef DATA_STATE_CLIENT_H
#define DATA_STATE_CLIENT_H

// Asks a host binary for its state dump: connect to its socket, send `state\n`, read text until
// the binary closes the connection (docs/trace.md, `avinspect.exe`). Bounded by a deadline, so a
// binary that hangs while dumping cannot hang the sampler.

#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Optional.h"
#include "core/Types.h"

namespace inspector {

// The dump's text; null, with `why` set, when it could not be had within `timeout_ms`.
Optional<String> FetchState(StringView socket_path, int timeout_ms, String& why);

} // namespace inspector

#endif
