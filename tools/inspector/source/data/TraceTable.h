#ifndef DATA_TRACE_TABLE_H
#define DATA_TRACE_TABLE_H

// One binary's trace table (docs/trace.md, "The table", version 1), decoded from a copy of its
// bytes. Every field is checked against the bytes held before it is read: the writer may be
// mid-update, or gone and its file truncated, and neither may make the reader read past the copy.
//
// Free slots are left out; a slot whose owner left every tracked function keeps its last function
// (`last_left`), which is what an idle thread was last doing.

#include "containers/DynamicArray.h"
#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Optional.h"
#include "core/Span.h"
#include "core/Types.h"

namespace inspector {

struct TraceFrame {
    u32 name_id = 0;
    u16 level = 0;     // the IRQL entered at; 0 in user mode
    u64 entered = 0;   // clock at entry, 100 ns
};

struct TraceSlot {
    u64 key = 0;           // the owner without the active bit
    bool active = false;   // inside a tracked function now
    u32 depth = 0;         // frames pushed; may exceed the frames stored
    u32 thread = 0;
    u64 entries = 0;       // functions entered on this slot, ever: progress
    u32 last_left = 0;     // name id of the outermost function most recently left
    u64 left_at = 0;
    u64 waiting_on = 0;    // the lock the thread is blocked on, 0 when none
    u64 holding = 0;       // the lock most recently acquired and still held
    DynamicArray<TraceFrame> frames; // outermost first
};

struct TraceTable {
    String module;
    u32 pid = 0;
    u64 created = 0;
    u64 ticks_per_second = 0;
    u32 crash_thread = 0;  // 0 until a crash
    u32 crash_code = 0;
    u64 crash_address = 0;
    u64 crash_clock = 0;
    u32 slot_count = 0;
    DynamicArray<TraceSlot> slots; // occupied slots only, in table order

    bool HasCrashed() const noexcept { return crash_thread != 0 || crash_code != 0; }
};

// Null, with `why` set, for bytes that are not a version 1 table.
Optional<TraceTable> ParseTraceTable(Span<const u8> bytes, String& why);

// The whole file at `path`; null, with `why` set, when it cannot be read.
Optional<DynamicArray<u8>> ReadWholeFile(StringView path, String& why);

} // namespace inspector

#endif
