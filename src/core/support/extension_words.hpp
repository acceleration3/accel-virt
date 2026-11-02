#ifndef AV_EXTENSION_WORDS_HPP
#define AV_EXTENSION_WORDS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;

class ExtensionWords {
public:
    ExtensionWords(BlockSource &blocks, Pool pool);
    ExtensionWords(const ExtensionWords &) = delete;
    ExtensionWords &operator=(const ExtensionWords &) = delete;

    [[nodiscard]] Status hold(const char *text, size_t bytes);
    [[nodiscard]] Status add(const char *name);
    void                 strike(const char *name);
    bool                 holds(const char *name) const;
    const char          *text() const;
    size_t               length() const;
    void                 clear();

    bool invariants_hold() const;

private:
    friend struct ExtensionWordsProbe;

    bool same(const char *word, size_t letters, const char *name) const;
    size_t word_end(size_t at) const;

    GrowableArray<char> text_;
};

}

#endif
