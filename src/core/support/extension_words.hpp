/* A space-separated string of extension names, read and edited a whole word
 * at a time: the CL_DEVICE_EXTENSIONS an executor shows a guest, the host's
 * list an OpenCL ICD reports from, the GL_EXTENSIONS string a compatibility
 * context answers.
 *
 * A word is matched whole - cl_khr_fp16 is not cl_khr_fp1 - and the string is
 * kept with one space between words, none at either end, and its terminator.
 * A host's string is read no further than the bytes it came in, since a host
 * that left out the terminator left out where its list ends. */
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

    /* The words of `text`, read no further than `bytes` nor past its
     * terminator, in place of what it held. no_memory leaves it empty. */
    [[nodiscard]] Status hold(const char *text, size_t bytes);
    /* `name` after the last word unless it is one already, which is ok too.
     * invalid_parameter for a null or empty name or one with a space in it,
     * no_memory when there is no room; either changes nothing. */
    [[nodiscard]] Status add(const char *name);
    /* `name` gone wherever it is a whole word. */
    void                 strike(const char *name);
    /* Whether `name` is a whole word of it. */
    bool                 holds(const char *name) const;
    /* The words, terminated; empty for none. */
    const char          *text() const;
    /* Its letters, the terminator not counted. */
    size_t               length() const;
    void                 clear();

    /* Terminated at its length, its words one space apart, none empty. */
    bool invariants_hold() const;

private:
    friend struct ExtensionWordsProbe;

    /* Whether the `letters` at `word` are the whole of `name`. */
    bool same(const char *word, size_t letters, const char *name) const;
    /* The end of the word starting at `at`. */
    size_t word_end(size_t at) const;

    /* The letters and their terminator; empty until a word is held. */
    GrowableArray<char> text_;
};

}  // namespace av

#endif  // AV_EXTENSION_WORDS_HPP
