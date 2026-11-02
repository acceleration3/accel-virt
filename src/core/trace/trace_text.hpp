#ifndef AV_TRACE_TEXT_HPP
#define AV_TRACE_TEXT_HPP

#include "core/trace/image_range.hpp"

#include <stdint.h>

namespace av {

class TextBuffer;
class TraceSlotView;
class TraceSnapshot;
struct TraceTextProbe;

class TraceText {
public:
    static constexpr uint32_t max_name = 128u;

    TraceText(const TraceSnapshot &snapshot, const ImageRange &image, uint64_t now);
    TraceText(const TraceText &) = delete;
    TraceText &operator=(const TraceText &) = delete;

    bool next(TextBuffer &out);

    bool invariants_hold() const;

private:
    enum class Part : uint8_t { header, crash, slot, frame, more, done };

    void header(TextBuffer &out) const;
    void crash(TextBuffer &out) const;
    void thread(TextBuffer &out, const TraceSlotView &view) const;
    void frame(TextBuffer &out, const TraceSlotView &view) const;
    void name(TextBuffer &out, uint32_t id, uint64_t text) const;
    void age(TextBuffer &out, const char *key, uint64_t since) const;
    void advance_slot();

    const TraceSnapshot &snapshot_;
    ImageRange           image_;
    uint64_t             now_;
    Part                 part_;
    uint32_t             slot_;
    uint32_t             frame_;

    friend struct TraceTextProbe;
};

}

#endif
