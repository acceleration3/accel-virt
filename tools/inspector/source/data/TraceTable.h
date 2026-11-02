#ifndef DATA_TRACE_TABLE_H
#define DATA_TRACE_TABLE_H

#include "containers/DynamicArray.h"
#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Optional.h"
#include "core/Span.h"
#include "core/Types.h"

namespace inspector {

struct TraceFrame {
    u32 name_id = 0;
    u16 level = 0;
    u64 entered = 0;
};

struct TraceSlot {
    u64 key = 0;
    bool active = false;
    u32 depth = 0;
    u32 thread = 0;
    u64 entries = 0;
    u32 last_left = 0;
    u64 left_at = 0;
    u64 waiting_on = 0;
    u64 holding = 0;
    DynamicArray<TraceFrame> frames;
};

struct TraceTable {
    String module;
    u32 pid = 0;
    u64 created = 0;
    u64 ticks_per_second = 0;
    u32 crash_thread = 0;
    u32 crash_code = 0;
    u64 crash_address = 0;
    u64 crash_clock = 0;
    u32 slot_count = 0;
    DynamicArray<TraceSlot> slots;

    bool HasCrashed() const noexcept { return crash_thread != 0 || crash_code != 0; }
};

Optional<TraceTable> ParseTraceTable(Span<const u8> bytes, String& why);

Optional<DynamicArray<u8>> ReadWholeFile(StringView path, String& why);

}

#endif
