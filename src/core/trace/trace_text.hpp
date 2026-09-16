/* A trace table as text, one line at a time, by the binary that owns it: the
 * crash file a process writes, the lines the driver writes to the debug port
 * at a bugcheck. Frame names are read through their pointers only when the
 * name lies inside the image and hashes to the frame's id; otherwise the id is
 * printed. Nothing is allocated and no lock is taken. */
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
    /* The longest name read through a frame's pointer. */
    static constexpr uint32_t max_name = 128u;

    TraceText(const TraceSnapshot &snapshot, const ImageRange &image, uint64_t now);
    TraceText(const TraceText &) = delete;
    TraceText &operator=(const TraceText &) = delete;

    /* The next line into `out`, which is cleared first by the caller's
     * choice of a fresh buffer; false once every line was written. */
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
    /* The next used slot at or after slot_, or the part after the slots. */
    void advance_slot();

    const TraceSnapshot &snapshot_;
    ImageRange           image_;
    uint64_t             now_;
    Part                 part_;
    uint32_t             slot_;
    uint32_t             frame_;

    friend struct TraceTextProbe;
};

}  // namespace av

#endif  // AV_TRACE_TEXT_HPP
