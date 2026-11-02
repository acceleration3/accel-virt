#ifndef DATA_SOURCES_H
#define DATA_SOURCES_H

#include "containers/DynamicArray.h"
#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Types.h"

namespace inspector {

struct Source {
    String module;
    u32 pid = 0;
    String trace_path;
    String state_path;
    bool alive = false;

    String Key() const;
};

DynamicArray<Source> ScanSources(StringView directory = "/dev/shm");

bool ProcessAlive(u32 pid);

}

#endif
