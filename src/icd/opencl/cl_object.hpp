/* One OpenCL object as the application holds it.
 *
 * Every handle this ICD hands out is a pointer to one of these, and the first
 * word of it is the dispatch table: that word is the whole of the ICD ABI, and
 * the Khronos loader reads it and jumps without checking anything. So it is
 * written before the object is handed over, always, whatever the object is.
 *
 * Underneath, an object is a number on the wire and a place in a tree: a
 * context's objects go when the context does, so each carries its parent.
 * Kinds are ordered so a parent's kind is always lower than its child's, which
 * is what bounds the chain. */
#ifndef AV_CLICD_CL_OBJECT_HPP
#define AV_CLICD_CL_OBJECT_HPP

#include <CL/cl.h>
#include <CL/cl_icd.h>

#include <stdint.h>

namespace av::clicd {

class GuestClContext;

class ClObject {
public:
    /* In dependency order: a parent's kind is lower than its children's, so
     * the chain from any object to its context is finite and short. */
    enum class Kind : uint8_t {
        platform, device, context, command_queue, program, mem, sampler, kernel, event
    };

    ClObject(const cl_icd_dispatch &table, Kind kind, uint64_t wire, GuestClContext *context,
             ClObject *parent);
    ClObject(const ClObject &) = delete;
    ClObject &operator=(const ClObject &) = delete;

    Kind            kind() const;
    uint64_t        wire() const;
    GuestClContext *context() const;
    ClObject       *parent() const;

    /* For a mem alone: where in its context's blob arena its memory is, and
     * how much of it. A buffer backed there is one the device made over the
     * same bytes, so a map hands the application this memory and an unmap
     * writes nothing back. `backed()` is false for every other kind and for
     * a buffer the arena had no room for, which is staged instead. */
    void     back_with(uint64_t offset, uint64_t bytes);
    bool     backed() const;
    uint64_t backing_offset() const;
    uint64_t backing_bytes() const;

    /* Whether this object is `other` or is somewhere below it. */
    bool descends_from(const ClObject &other) const;

    /* The application's own reference count, which is not the host's: the
     * guest retains and releases, and only the last release destroys. */
    void     retain();
    /* Whether that was the last one. */
    bool     release();
    uint32_t references() const;

    bool invariants_hold() const;

private:
    /* First, and it stays first: the loader reads this word and calls through
     * it. Nothing may be declared before it. */
    const cl_icd_dispatch *dispatch_;
    GuestClContext        *context_;
    ClObject              *parent_;
    uint64_t               wire_;
    uint64_t               backing_offset_;
    uint64_t               backing_bytes_;
    uint32_t               references_;
    Kind                   kind_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_OBJECT_HPP
