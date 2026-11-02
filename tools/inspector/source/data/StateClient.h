#ifndef DATA_STATE_CLIENT_H
#define DATA_STATE_CLIENT_H

#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Optional.h"
#include "core/Types.h"

namespace inspector {
Optional<String> FetchState(StringView socket_path, int timeout_ms, String& why);

}

#endif
