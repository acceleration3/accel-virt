#include "data/TraceTable.h"

#include "text/Format.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace inspector {
namespace {

constexpr u64 kMagic = 0x0045434152545641ull;
constexpr u32 kVersion = 1;
constexpr usize kHeaderBytes = 96;
constexpr usize kSlotHeaderBytes = 56;
constexpr usize kFrameBytes = 24;
constexpr u64 kActiveBit = 1ull << 63;
constexpr u32 kMaxSlots = 4096;
constexpr u32 kMaxFrames = 1024;

template <typename T>
T ReadAt(Span<const u8> bytes, usize offset) {
    T value{};
    memcpy(&value, bytes.GetBuffer() + offset, sizeof(T));
    return value;
}

String ModuleName(Span<const u8> bytes) {
    String name;
    for (usize i = 32; i < 48 && bytes[i] != 0; ++i) {
        name.Append(static_cast<char>(bytes[i]));
    }
    return name;
}

TraceSlot DecodeSlot(Span<const u8> slot, u32 frames_per_slot) {
    TraceSlot s;
    const u64 owner = ReadAt<u64>(slot, 0);
    s.key = owner & ~kActiveBit;
    s.active = (owner & kActiveBit) != 0;
    s.depth = ReadAt<u32>(slot, 8);
    s.thread = ReadAt<u32>(slot, 12);
    s.entries = ReadAt<u64>(slot, 16);
    s.last_left = ReadAt<u32>(slot, 24);
    s.left_at = ReadAt<u64>(slot, 32);
    s.waiting_on = ReadAt<u64>(slot, 40);
    s.holding = ReadAt<u64>(slot, 48);
    const u32 stored = s.depth < frames_per_slot ? s.depth : frames_per_slot;
    for (u32 f = 0; s.active && f < stored; ++f) {
        const usize at = kSlotHeaderBytes + static_cast<usize>(f) * kFrameBytes;
        TraceFrame frame;
        frame.name_id = ReadAt<u32>(slot, at);
        frame.level = ReadAt<u16>(slot, at + 4);
        frame.entered = ReadAt<u64>(slot, at + 8);
        s.frames.PushBack(frame);
    }
    return s;
}

}

Optional<TraceTable> ParseTraceTable(Span<const u8> bytes, String& why) {
    if (bytes.GetSize() < kHeaderBytes) {
        why = Format("{} bytes, shorter than a header", bytes.GetSize());
        return NullOpt;
    }
    if (ReadAt<u64>(bytes, 0) != kMagic) {
        why = "no AVTRACE magic";
        return NullOpt;
    }
    const u32 version = ReadAt<u32>(bytes, 8);
    const u32 header_bytes = ReadAt<u32>(bytes, 12);
    const u32 slot_count = ReadAt<u32>(bytes, 16);
    const u32 slot_bytes = ReadAt<u32>(bytes, 20);
    const u32 frames_per_slot = ReadAt<u32>(bytes, 24);
    const u32 frame_bytes = ReadAt<u32>(bytes, 28);
    if (version != kVersion || header_bytes != kHeaderBytes || frame_bytes != kFrameBytes) {
        why = Format("version {}, header {} bytes, frame {} bytes: not a version 1 table", version,
                     header_bytes, frame_bytes);
        return NullOpt;
    }
    if (slot_count > kMaxSlots || frames_per_slot > kMaxFrames ||
        slot_bytes != kSlotHeaderBytes + static_cast<usize>(frames_per_slot) * kFrameBytes) {
        why = Format("{} slots of {} bytes with {} frames do not add up", slot_count, slot_bytes, frames_per_slot);
        return NullOpt;
    }
    const usize needed = kHeaderBytes + static_cast<usize>(slot_count) * slot_bytes;
    if (bytes.GetSize() < needed) {
        why = Format("{} bytes, the table needs {}", bytes.GetSize(), needed);
        return NullOpt;
    }

    TraceTable table;
    table.module = ModuleName(bytes);
    table.pid = ReadAt<u32>(bytes, 48);
    table.crash_thread = ReadAt<u32>(bytes, 52);
    table.created = ReadAt<u64>(bytes, 56);
    table.ticks_per_second = ReadAt<u64>(bytes, 64);
    table.crash_code = ReadAt<u32>(bytes, 72);
    table.crash_address = ReadAt<u64>(bytes, 80);
    table.crash_clock = ReadAt<u64>(bytes, 88);
    table.slot_count = slot_count;
    for (u32 i = 0; i < slot_count; ++i) {
        const Span<const u8> slot = bytes.Subspan(kHeaderBytes + static_cast<usize>(i) * slot_bytes, slot_bytes);
        if (ReadAt<u64>(slot, 0) == 0) {
            continue;
        }
        table.slots.PushBack(DecodeSlot(slot, frames_per_slot));
    }
    return table;
}

Optional<DynamicArray<u8>> ReadWholeFile(StringView path, String& why) {
    const String terminated(path);
    const int fd = open(terminated.CStr(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        why = Format("cannot open {}: {}", path, strerror(errno));
        return NullOpt;
    }
    DynamicArray<u8> bytes;
    u8 chunk[65536];
    for (;;) {
        const ssize_t got = read(fd, chunk, sizeof chunk);
        if (got < 0) {
            why = Format("cannot read {}: {}", path, strerror(errno));
            close(fd);
            return NullOpt;
        }
        if (got == 0) {
            break;
        }
        bytes.Append(chunk, static_cast<usize>(got));
    }
    close(fd);
    return bytes;
}

}
