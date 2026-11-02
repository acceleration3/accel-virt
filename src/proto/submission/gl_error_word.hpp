#ifndef AV_GL_ERROR_WORD_HPP
#define AV_GL_ERROR_WORD_HPP

#include <stdint.h>

namespace av {

class GlErrorWord {
public:
    static constexpr uint32_t first_error = 0x0500u;
    static constexpr uint32_t kinds = 7u;

    GlErrorWord();
    explicit GlErrorWord(uint64_t word);

    void     record(uint32_t error);
    bool     published() const;
    uint8_t  count(uint32_t kind) const;
    uint64_t word() const;

private:
    uint64_t word_;
};

}

#endif
