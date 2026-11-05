#ifndef AV_STATE_WRITER_HPP
#define AV_STATE_WRITER_HPP

#include "core/support/text_buffer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct StateWriterProbe;

class StateWriter {
public:
    static constexpr uint32_t max_depth = 8u;

    StateWriter(char *destination, size_t capacity);
    StateWriter(const StateWriter &) = delete;
    StateWriter &operator=(const StateWriter &) = delete;

    void open(const char *kind, uint64_t index);
    void close();
    void number(const char *key, uint64_t value);
    void hex(const char *key, uint64_t value);
    void address(const char *key, const void *object);
    void text(const char *key, const char *value);
    void flag(const char *key, bool value);

    const char *dump() const;
    size_t      length() const;
    bool        truncated() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t hex_digits = 16u;

    void key(const char *name);
    void append_escaped(const char *value);

    TextBuffer  out_;
    size_t      capacity_;
    const char *kinds_[max_depth];
    uint64_t    indices_[max_depth];
    uint32_t    depth_;
    uint32_t    opened_;

    friend struct StateWriterProbe;
};

}

#endif
